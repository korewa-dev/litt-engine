#version 460
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 uView;
    mat4 uProj;
    mat4 uModel;
} uPush;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vScreenPos; // Pass screen position for dithering

void main() {
    gl_Position = uPush.uProj * uPush.uView * uPush.uModel * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    // Pass clip-space position for dither3D derivative computation
    vScreenPos = gl_Position;
}
