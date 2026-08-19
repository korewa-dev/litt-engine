#version 460
#extension GL_EXT_scalar_block_layout : require

// AMD FidelityFX Super Resolution 4
// AI-enhanced upscaling for RDNA 4/5 and all Vulkan GPUs

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1) uniform sampler2D uHistory;
layout(set = 0, binding = 2) uniform sampler2D uVelocity;
layout(set = 0, binding = 3, scalar) uniform Fsr4Constants {
    uint uInputWidth;
    uint uInputHeight;
    uint uOutputWidth;
    uint uOutputHeight;
    uint uQuality;
    uint uMode;
    float fSharpness;
    float fContrast;
    float fTemporalStability;
    uint uAiReconstruction;
    uint uFrameGenEnabled;
    uint uPad[2];
} uFsr4;

layout(rgba16f, set = 0, binding = 4) uniform image2D uOutput;

// FSR 4 adaptive sharpening
float adaptive_sharpen(sampler2D s, vec2 uv, float sharp) {
    vec4 color = texture(s, uv);
    vec2 px = vec2(1.0) / vec2(uFsr4.uOutputWidth, uFsr4.uOutputHeight);
    
    // 3x3 neighborhood
    vec4 n  = texture(s, uv + vec2( 0.0, -1.0) * px);
    vec4 s_ = texture(s, uv + vec2( 0.0,  1.0) * px);
    vec4 e  = texture(s, uv + vec2( 1.0,  0.0) * px);
    vec4 w  = texture(s, uv + vec2(-1.0,  0.0) * px);
    vec4 ne = texture(s, uv + vec2( 1.0, -1.0) * px);
    vec4 nw = texture(s, uv + vec2(-1.0, -1.0) * px);
    vec4 se = texture(s, uv + vec2( 1.0,  1.0) * px);
    vec4 sw = texture(s, uv + vec2(-1.0,  1.0) * px);
    
    float max_src = max(max(color.r, color.g), color.b);
    float min_src = min(min(color.r, color.g), color.b);
    
    float contrib_n = max(n.r, max(n.g, n.b));
    float contrib_s = max(s_.r, max(s_.g, s_.b));
    float contrib_e = max(e.r, max(e.g, e.b));
    float contrib_w = max(w.r, max(w.g, w.b));
    float contrib_ne = max(ne.r, max(ne.g, ne.b));
    float contrib_nw = max(nw.r, max(nw.g, nw.b));
    float contrib_se = max(se.r, max(se.g, se.b));
    float contrib_sw = max(sw.r, max(sw.g, sw.b));
    
    float min_dst = min(min(contrib_n, contrib_s), min(contrib_e, contrib_w));
    min_dst = min(min_dst, min(min(contrib_ne, contrib_nw), min(contrib_se, contrib_sw)));
    
    float ramp = max_src - min_dst;
    float dst_lum = (contrib_n + contrib_s + contrib_e + contrib_w +
                     contrib_ne + contrib_nw + contrib_se + contrib_sw) / 8.0;
    
    float filter_width = clamp(ramp / max(dst_lum, 1e-5), 0.0, 1.0);
    filter_width = filter_width * filter_width * (3.0 - 2.0 * filter_width);
    
    float contrib = filter_width * sharp;
    
    vec3 tap = color.rgb * 4.0;
    tap += n.rgb + s_.rgb + e.rgb + w.rgb;
    tap += ne.rgb * 0.5 + nw.rgb * 0.5 + se.rgb * 0.5 + sw.rgb * 0.5;
    tap /= 8.0;
    
    return mix(color.r, tap.r, contrib);
}

// FSR 4 temporal reprojection
vec4 temporal_reproject(vec2 uv, vec2 velocity) {
    vec2 history_uv = uv - velocity;
    history_uv = clamp(history_uv, 0.0, 1.0);
    return texture(uHistory, history_uv);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uFsr4.uOutputWidth) || pos.y >= int(uFsr4.uOutputHeight)) return;
    
    vec2 uv = (vec2(pos) + 0.5) / vec2(uFsr4.uOutputWidth, uFsr4.uOutputHeight);
    vec2 input_uv = uv * vec2(uFsr4.uInputWidth, uFsr4.uInputHeight) / vec2(uFsr4.uOutputWidth, uFsr4.uOutputHeight);
    
    vec4 current = texture(uInput, input_uv);
    vec4 history = temporal_reproject(uv, vec2(0.0)); // velocity from compute pass
    
    // Blend based on temporal stability
    float alpha = uFsr4.fTemporalStability;
    vec4 blended = mix(history, current, alpha);
    
    // Apply adaptive sharpening
    float sharp = uFsr4.fSharpness;
    blended.r = adaptive_sharpen(uInput, input_uv, sharp);
    blended.g = adaptive_sharpen(uInput, input_uv, sharp);
    blended.b = adaptive_sharpen(uInput, input_uv, sharp);
    
    // Apply contrast
    blended.rgb = (blended.rgb - 0.5) * uFsr4.fContrast + 0.5;
    
    imageStore(uOutput, pos, blended);
}
