#version 460

// Studio vertex shader: position + color, one MVP push constant.
// Used for BOTH the 3D world viewport and the 2D chat panel
// (the panel supplies a pixel-space ortho matrix).

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;

layout(push_constant) uniform PC {
    mat4 uMvp;
} pc;

layout(location = 0) out vec3 vCol;

void main() {
    gl_Position = pc.uMvp * vec4(aPos, 1.0);
    vCol = aCol;
}
