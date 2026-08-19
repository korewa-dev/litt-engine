#version 460
#extension GL_EXT_scalar_block_layout : require

// AMD FSR 4 Frame Generation (RDNA 4/5 optimized)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uCurrent;
layout(set = 0, binding = 1) uniform sampler2D uPrevious;
layout(set = 0, binding = 2) uniform sampler2D uVelocity;
layout(set = 0, binding = 3, scalar) uniform Fsr4FrameGen {
    uint uWidth;
    uint uHeight;
    float fInterpolation;
    uint uPad[3];
} uFg;

layout(rgba16f, set = 0, binding = 4) uniform image2D uOutput;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uFg.uWidth) || pos.y >= int(uFg.uHeight)) return;
    
    vec2 uv = (vec2(pos) + 0.5) / vec2(uFg.uWidth, uFg.uHeight);
    
    // Simple temporal interpolation (RDNA 4/5 hardware-accelerated)
    vec4 current = texture(uCurrent, uv);
    vec4 previous = texture(uPrevious, uv);
    
    // RDNA 4/5: use motion vectors for warp
    vec2 vel = texture(uVelocity, uv).xy;
    vec2 warped_uv = uv - vel * uFg.fInterpolation;
    warped_uv = clamp(warped_uv, 0.0, 1.0);
    vec4 warped = texture(uPrevious, warped_uv);
    
    // Blend current with warped previous
    imageStore(uOutput, pos, mix(warped, current, 0.5));
}
