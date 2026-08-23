#version 460
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1, scalar) uniform Constants {
    vec3 uAlbedo;
    float uMetallic;
    float uRoughness;
    uint uPad;
} uConstants;

void main() {
    vec3 albedo = texture(uTexture, vTexCoord).rgb * uConstants.uAlbedo;
    oColor = vec4(albedo, 1.0);
}
