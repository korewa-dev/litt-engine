#version 450
#extension GL_EXT_scalar_block_layout : require
// Compute atlas for RT instance buffer
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0) buffer Atlas { uint data[]; } uAtlas;
layout(set = 0, binding = 1, scalar) uniform Constants { uint uCount; } uConstants;
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uConstants.uCount) return;
    uAtlas.data[i] = i;
}
