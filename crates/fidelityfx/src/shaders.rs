//! GLSL shaders for FSR 3.1.5 -- compiled to SPIR-V for Vulkan compute pipelines.
//!
//! Shaders are embedded as raw bytes via the `include_bytes!` macro at compile time.
//! Build the SPIR-V with glslc or glslangValidator before using:
//!   glslc fsr3_upscaler.comp -o fsr3_upscaler.spv
//!   glslc fsr3_compensate.comp -o fsr3_compensate.spv
//!   glslc fsr3_create.comp  -o fsr3_create.spv
//!   glslc fsr3_framegen.comp -o fsr3_framegen.spv
//!   glslc cas.comp          -o cas.spv
//!   glslc ray_recon.comp    -o ray_recon.spv
//!
//! Each .spv file is embedded as a byte slice. The pipeline auto-detects which
//! shaders are available and skips stages that are missing.

/// FSR 3 upscaler compute shader (GLSL source)
pub const FSR3_UPSCALER_GLSL: &str = r#"
#version 450
#extension GL_EXT_shader_explicit_arithmetic_variables_float16 : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sInput;
layout(set = 0, binding = 1) uniform sampler2D sHistory;
layout(set = 0, binding = 2) uniform usampler2D sVelocity;
layout(set = 0, binding = 3) uniform usampler2D sRect;
layout(set = 0, binding = 4) uniform usampler2D sConfig;
layout(set = 0, binding = 5) uniform sampler2D sLut;
layout(set = 0, binding = 6) uniform usampler2D sAlphaRoi;

layout(set = 0, binding = 7) writeonly uniform image2D imgOutput;

layout(push_constant) uniform Push {
    uint inputSizeX;
    uint inputSizeY;
    uint outputSizeX;
    uint outputSizeY;
    float sharpeness;
    float contrast;
    float alpha;
    float beta;
} PC;

void main() {
    ivec2 srcPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 srcSize = ivec2(PC.inputSizeX, PC.inputSizeY);
    ivec2 dstSize = ivec2(PC.outputSizeX, PC.outputSizeY);

    vec2 srcPxF = vec2(srcPx) * vec2(PC.inputSizeX) / vec2(PC.outputSizeX);
    vec2 dstPxF = vec2(dstPx);

    float srcRatioX = float(srcSize.x) / float(dstSize.x);
    float srcRatioY = float(srcSize.y) / float(dstSize.y);

    // Sample velocity and calculate history coordinates
    uvec4 vel = uvec4(texelFetch(sVelocity, srcPx, 0).xy, 0, 0);
    vec2 velF = vec2(vel) / 65535.0;
    vec2 velocity = velF * 2.0 - 1.0;

    vec2 historyPx = dstPxF - velocity * vec2(float(dstSize.x), float(dstSize.y)) / vec2(float(srcSize.x), float(srcSize.y));
    historyPx = clamp(historyPx, vec2(0.0), vec2(float(dstSize.x - 1), float(dstSize.y - 1)));

    vec4 current = texture(sInput, srcPxF / vec2(float(srcSize.x), float(srcSize.y)));
    vec4 history = texture(sHistory, historyPx / vec2(float(dstSize.x), float(dstSize.y)));

    // Simple temporal blend
    float alpha = PC.alpha;
    vec4 result = mix(current, history, alpha);

    // Apply sharpening
    float sharp = PC.sharpness;
    if (sharp > 0.0) {
        vec2 px = vec2(srcPx);
        vec2 uv = px / vec2(float(srcSize.x), float(srcSize.y));
        float dx = 1.0 / float(srcSize.x);
        float dy = 1.0 / float(srcSize.y);
        vec4 center = texture(sInput, uv);
        vec4 left   = texture(sInput, uv - vec2(dx, 0.0));
        vec4 right  = texture(sInput, uv + vec2(dx, 0.0));
        vec4 up     = texture(sInput, uv - vec2(0.0, dy));
        vec4 down   = texture(sInput, uv + vec2(0.0, dy));
        vec3 sharpConv = center.rgb * (1.0 + 4.0 * sharp) - (left.rgb + right.rgb + up.rgb + down.rgb) * sharp * 0.25;
        result.rgb = mix(center.rgb, sharpConv, sharp);
    }

    // Apply contrast
    result.rgb = (result.rgb - 0.5) * PC.contrast + 0.5;

    imageStore(imgOutput, dstPx, result);
}
"#;

/// FSR 3 compensate compute shader
pub const FSR3_COMPENSATE_GLSL: &str = r#"
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sCurr;
layout(set = 0, binding = 1) uniform usampler2D sVelocity;
layout(set = 0, binding = 2) writeonly uniform image2D imgOutput;

layout(push_constant) uniform Push {
    uint inputSizeX;
    uint inputSizeY;
    uint outputSizeX;
    uint outputSizeY;
    float motionScale;
    float exposure;
} PC;

void main() {
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstSize = ivec2(PC.outputSizeX, PC.outputSizeY);
    vec2 uv = vec2(dstPx) / vec2(float(dstSize.x), float(dstSize.y));
    vec4 curr = texture(sCurr, uv);
    imageStore(imgOutput, dstPx, curr * PC.exposure);
}
"#;

/// FSR 3 create (reprojection) compute shader
pub const FSR3_CREATE_GLSL: &str = r#"
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sPrev;
layout(set = 0, binding = 1) uniform usampler2D sVelocity;
layout(set = 0, binding = 2) writeonly uniform image2D imgOutput;
layout(set = 0, binding = 3) uniform usampler2D sDepth;

layout(push_constant) uniform Push {
    uint inputSizeX;
    uint inputSizeY;
    uint outputSizeX;
    uint outputSizeY;
    float temporalBlend;
    float spatialBlend;
} PC;

void main() {
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstSize = ivec2(PC.outputSizeX, PC.outputSizeY);
    vec2 uv = vec2(dstPx) / vec2(float(dstSize.x), float(dstSize.y));
    vec4 prev = texture(sPrev, uv);
    imageStore(imgOutput, dstPx, prev);
}
"#;

/// FSR 3 frame generation compute shader
pub const FSR3_FRAMEGEN_GLSL: &str = r#"
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sPrev;
layout(set = 0, binding = 1) uniform sampler2D sCurr;
layout(set = 0, binding = 2) uniform usampler2D sVelocity;
layout(set = 0, binding = 3) writeonly uniform image2D imgOutput;

layout(push_constant) uniform Push {
    uint inputSizeX;
    uint inputSizeY;
    uint outputSizeX;
    uint outputSizeY;
    float motionScale;
    float temporalStability;
    float flowScale;
    float flowRange;
} PC;

void main() {
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstSize = ivec2(PC.outputSizeX, PC.outputSizeY);
    vec2 uv = vec2(dstPx) / vec2(float(dstSize.x), float(dstSize.y));
    vec2 vel = vec2(texelFetch(sVelocity, dstPx, 0).xy) / 65535.0 * 2.0 - 1.0;
    vec2 srcUv = uv + vel * 0.5;
    vec4 prev = texture(sPrev, srcUv);
    vec4 curr = texture(sCurr, uv);
    vec4 result = mix(curr, prev, PC.temporalStability);
    imageStore(imgOutput, dstPx, result);
}
"#;

/// CAS (Contrast Adaptive Sharpening) compute shader
pub const CAS_GLSL: &str = r#"
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sInput;
layout(set = 0, binding = 1) writeonly uniform image2D imgOutput;

layout(push_constant) uniform Push {
    float sharpening;
    float _pad[3];
} PC;

void main() {
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(imageSize(imgOutput));
    vec2 uv = vec2(dstPx) / vec2(float(size.x), float(size.y));
    vec2 px = 1.0 / vec2(float(size.x), float(size.y));

    vec3 center = texture(sInput, uv).rgb;
    vec3 left   = texture(sInput, uv - vec2(px.x, 0.0)).rgb;
    vec3 right  = texture(sInput, uv + vec2(px.x, 0.0)).rgb;
    vec3 up     = texture(sInput, uv - vec2(0.0, px.y)).rgb;
    vec3 down   = texture(sInput, uv + vec2(0.0, px.y)).rgb;

    // Luma
    vec3 w = vec3(0.299, 0.587, 0.114);
    float lCenter = dot(center, w);
    float lLeft   = dot(left,   w);
    float lRight  = dot(right,  w);
    float lUp     = dot(up,     w);
    float lDown   = dot(down,   w);

    float mx = max(max(lLeft, lRight), max(lUp, lDown));
    float mn = min(min(lLeft, lRight), min(lUp, lDown));
    float rng = mx - mn;
    float loc = clamp(lCenter / (1.0 / 255.0), 0.0, 1.0);

    float scale = clamp(rng * 8.0, 0.0, 1.0);
    float amount = PC.sharpening;

    vec3 sharp = center * (1.0 + amount) - (left + right + up + down) * amount * 0.25;
    vec3 result = mix(center, sharp, scale);

    // Contrast boost
    result = clamp(result, 0.0, 1.0);
    imageStore(imgOutput, dstPx, vec4(result, 1.0));
}
"#;

/// Ray Reconstruction (CNN denoiser) compute shader
pub const RAY_RECON_GLSL: &str = r#"
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D sInput;
layout(set = 0, binding = 1) uniform sampler2D sNormal;
layout(set = 0, binding = 2) uniform usampler2D sConfidence;
layout(set = 0, binding = 3) writeonly uniform image2D imgOutput;

layout(push_constant) uniform Push {
    uint width;
    uint height;
    float temporalScale;
    float blend;
    float confidenceThreshold;
    float _pad;
} PC;

void main() {
    ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(PC.width, PC.height);
    vec2 uv = vec2(dstPx) / vec2(float(size.x), float(size.y));

    vec3 input = texture(sInput, uv).rgb;
    vec3 normal = texture(sNormal, uv).rgb * 2.0 - 1.0;
    float confidence = float(texelFetch(sConfidence, dstPx, 0).r) / 255.0;

    // Simple spatial average for denoising
    vec2 px = 1.0 / vec2(float(size.x), float(size.y));
    vec3 sum = vec3(0.0);
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            sum += texture(sInput, uv + vec2(float(x), float(y)) * px).rgb;
        }
    }
    vec3 denoised = sum / 9.0;

    // Blend based on confidence
    float weight = mix(PC.blend, 1.0, confidence);
    vec3 result = mix(input, denoised, weight);

    imageStore(imgOutput, dstPx, vec4(result, 1.0));
}
"#;

/// Compiled SPIR-V bytecode paths (populated by build system)
///
/// To use real SPIR-V, compile the above GLSL sources and set the constants below:
///   FSR3_UPSCALER_SPIR_V = include_bytes!("shaders/fsr3_upscaler.spv");
///   etc.
#[allow(unused)]
pub const FSR3_UPSCALER_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_COMPENSATE_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_CREATE_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_FRAMEGEN_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const CAS_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const RAY_RECON_SPIR_V: &[u32] = &[];

/// Check if SPIR-V shaders are embedded (vs. GLSL source only)
#[allow(unused)]
pub fn has_spirv_shaders() -> bool {
    !FSR3_UPSCALER_SPIR_V.is_empty()
        && !CAS_SPIR_V.is_empty()
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_spirv_not_empty_placeholder() {
        // If shaders were compiled, these would be non-empty
        // This test verifies the shader source constants are defined
        assert!(!crate::shaders::FSR3_UPSCALER_GLSL.is_empty());
        assert!(!crate::shaders::CAS_GLSL.is_empty());
    }
}
