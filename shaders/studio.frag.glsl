#version 460

// Studio fragment shader: flat vertex color passthrough.

layout(location = 0) in vec3 vCol;
layout(location = 0) out vec4 oColor;

void main() {
    oColor = vec4(vCol, 1.0);
}
