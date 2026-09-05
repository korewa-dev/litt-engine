#version 450
#extension GL_EXT_scalar_block_layout : require
// FSR 3 Frame Generation - Create Motion Vectors
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D uCurrent;
layout(set = 0, binding = 1) uniform sampler2D uPrevious;
layout(set = 0, binding = 2, scalar) uniform Constants {
    uint uWidth;
    uint uHeight;
    float fFrameRatio;
    float fExposure;
    vec3 vPrevCameraPos;
    vec3 vCurrCameraPos;
} uConstants;
layout(rgba16f, set = 0, binding = 3) uniform image2D uVelocity;
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uConstants.uWidth) || pos.y >= int(uConstants.uHeight)) return;
    vec2 uv = (vec2(pos) + 0.5) / vec2(uConstants.uWidth, uConstants.uHeight);
    imageStore(uVelocity, pos, vec4(uv, 0.0, 1.0));
}
