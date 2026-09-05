#version 450
#extension GL_EXT_scalar_block_layout : require
// FidelityFX Specular Denoiser stub
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1, scalar) uniform Constants {
    uint uWidth;
    uint uHeight;
    float fTemporalScale;
    float fBlend;
    float fHitRate;
    uint uPad;
} uConstants;
layout(rgba16f, set = 0, binding = 2) uniform image2D uOutput;
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uConstants.uWidth) || pos.y >= int(uConstants.uHeight)) return;
    vec2 uv = (vec2(pos) + 0.5) / vec2(uConstants.uWidth, uConstants.uHeight);
    imageStore(uOutput, pos, texture(uInput, uv));
}
