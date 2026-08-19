#version 450
#extension GL_EXT_scalar_block_layout : require
// FSR 3 Frame Generation - Upscaler
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1) uniform sampler2D uVelocity;
layout(set = 0, binding = 2, scalar) uniform Constants {
    uint uInputWidth;
    uint uInputHeight;
    uint uOutputWidth;
    uint uOutputHeight;
} uConstants;
layout(rgba16f, set = 0, binding = 3) uniform image2D uOutput;
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uConstants.uOutputWidth) || pos.y >= int(uConstants.uOutputHeight)) return;
    vec2 uv = (vec2(pos) + 0.5) / vec2(uConstants.uOutputWidth, uConstants.uOutputHeight);
    vec2 input_uv = uv * vec2(uConstants.uInputWidth, uConstants.uInputHeight) / vec2(uConstants.uOutputWidth, uConstants.uOutputHeight);
    imageStore(uOutput, pos, texture(uInput, input_uv));
}
