#version 450
#extension GL_EXT_scalar_block_layout : require

// FSR 2 upscaler stub
// Full implementation requires the FSR2 SDK compute shaders

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1) uniform sampler2D uHistory;
layout(set = 0, binding = 2) uniform sampler2D uVelocity;
layout(set = 0, binding = 3, scalar) uniform Fsr2Constants {
    uint uInputWidth;
    uint uInputHeight;
    uint uOutputWidth;
    uint uOutputHeight;
    float fExposure;
    float fSharpness;
    float fMotionScale;
    float fPad;
    float fFrameTime;
    float fPad2[3];
} uFsr2;
layout(set = 0, binding = 4, scalar) uniform Flags {
    uint uFlags;
} uFlags;

layout(rgba16f, set = 0, binding = 5) uniform image2D uOutput;

// Minimal FSR2 fallback: bilinear upscale
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uFsr2.uOutputWidth) || pos.y >= int(uFsr2.uOutputHeight)) return;
    
    vec2 uv = (vec2(pos) + 0.5) / vec2(uFsr2.uOutputWidth, uFsr2.uOutputHeight);
    // Map to input space
    vec2 input_uv = uv * vec2(uFsr2.uInputWidth, uFsr2.uInputHeight) / vec2(uFsr2.uOutputWidth, uFsr2.uOutputHeight);
    
    vec4 color = texture(uInput, input_uv);
    imageStore(uOutput, pos, color);
}
