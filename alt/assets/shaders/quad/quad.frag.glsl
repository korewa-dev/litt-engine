#version 450
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

void main() {
    oColor = texture(uTexture, vUv);
}
