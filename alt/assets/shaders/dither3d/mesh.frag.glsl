#version 460
#extension GL_EXT_scalar_block_layout : require

#include "../dither3d/include.glsl"

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vScreenPos;     // Screen-space position for dithering
layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1, scalar) uniform Constants {
    vec3 uAlbedo;
    float uMetallic;
    float uRoughness;
    uint uPad;
    // Dither3D parameters
    float uDitherEnabled;    // 0=disabled, 1=enabled
    float uDitherScale;
    float uDitherSizeVar;
    float uDitherContrast;
    uint uDitherColorMode;   // 0=Gray, 1=RGB, 2=CMYK
    uint uDitherPattern;     // 0=1x1, 1=2x2, 2=4x4, 3=8x8
    float uDitherInputExp;
    float uDitherInputOff;
} uConstants;

void main() {
    vec3 albedo = texture(uTexture, vTexCoord).rgb * uConstants.uAlbedo;
    float metallic = clamp(uConstants.uMetallic, 0.0, 1.0);
    float roughness = clamp(uConstants.uRoughness, 0.0, 1.0);
    // With this compact forward shader there is no normal/light input yet.
    // Still preserve the authored PBR response instead of discarding it:
    // dielectrics retain diffuse energy, metals shift energy into F0, and
    // roughness attenuates the environment/specular contribution.
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 diffuse = albedo * (1.0 - metallic);
    vec3 surface = diffuse + f0 * (1.0 - roughness) * 0.35;

    // Apply dithering if enabled
    if (uConstants.uDitherEnabled > 0.5) {
        vec3 dithered = dither3D(
            vTexCoord,
            vScreenPos,
            surface
        );
        oColor = vec4(dithered, 1.0);
    } else {
        oColor = vec4(surface, 1.0);
    }
}
