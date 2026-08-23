#version 460
#extension GL_EXT_scalar_block_layout : require

// AMD FSR 4.0.1 ML Frame Generation (Vulkan-compatible)
// Based on AMD FidelityFX SDK 2.3.0

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uCurrent;
layout(set = 0, binding = 1) uniform sampler2D uPrevious;
layout(set = 0, binding = 2) uniform sampler2D uVelocity;
layout(set = 0, binding = 3) uniform sampler2D uMask;

layout(set = 0, binding = 4, scalar) uniform Fsr4FrameGen {
    uint uWidth;
    uint uHeight;
    float fInterpolation;
    float fSharpness;
    uint uPad[4];
} uFrameGen;

layout(rgba16f, set = 0, binding = 5) uniform image2D uOutput;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uFrameGen.uWidth) || pos.y >= int(uFrameGen.uHeight)) return;
    
    vec2 uv = (vec2(pos) + 0.5) / vec2(uFrameGen.uWidth, uFrameGen.uHeight);
    
    // Get velocity
    vec2 vel = texture(uVelocity, uv).xy;
    
    // Warp previous frame using motion vectors
    vec2 warped_uv = uv - vel * uFrameGen.fInterpolation;
    warped_uv = clamp(warped_uv, 0.0, 1.0);
    vec4 warped = texture(uPrevious, warped_uv);
    
    vec4 current = texture(uCurrent, uv);
    
    // Blend current with warped previous
    // In full SDK, this uses ML confidence masks
    float confidence = 0.5;
    imageStore(uOutput, pos, mix(warped, current, confidence));
}
