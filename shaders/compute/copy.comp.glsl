#version 450
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uSrc;
layout(set = 0, binding = 1) uniform sampler2D uDst;
layout(set = 0, binding = 2) uniform usampler2D uIndices;

layout(set = 0, binding = 3, scalar) uniform Constants {
    uint uWidth;
    uint uHeight;
} uConstants;

layout(rgba16f, set = 0, binding = 4) uniform image2D uOutput;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= int(uConstants.uWidth) || pos.y >= int(uConstants.uHeight)) return;
    
    vec4 color = texture(uSrc, vec2(pos) / vec2(uConstants.uWidth, uConstants.uHeight));
    imageStore(uOutput, pos, color);
}
