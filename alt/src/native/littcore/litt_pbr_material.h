// Phase 3: Rendering Pipeline - Material System Extension (PBR + Textures)
// PBRMaterial is defined in litt_material.h - this header provides texture integration.

#pragma once
#include "litt_math.h"
#include "litt_texture.h"
#include "litt_material.h"
#include <string>
#include <memory>

namespace litt {

// Material factory
class MaterialFactory {
public:
    // Create default PBR material
    static std::unique_ptr<PBRMaterial> create_default() {
        return std::make_unique<PBRMaterial>();
    }

    // Create metallic material
    static std::unique_ptr<PBRMaterial> create_metallic(const Vec3& color, float roughness = 0.1f) {
        auto material = create_default();
        material->albedo = color;
        material->metallic = 1.0f;
        material->roughness = roughness;
        return material;
    }

    // Create dielectric material
    static std::unique_ptr<PBRMaterial> create_dielectric(const Vec3& color, float roughness = 0.5f) {
        auto material = create_default();
        material->albedo = color;
        material->metallic = 0.0f;
        material->roughness = roughness;
        return material;
    }

    // Create emissive material
    static std::unique_ptr<PBRMaterial> create_emissive(const Vec3& color, float intensity = 1.0f) {
        auto material = create_default();
        material->albedo = color;
        material->emission = color * intensity;
        return material;
    }
};

} // namespace litt
