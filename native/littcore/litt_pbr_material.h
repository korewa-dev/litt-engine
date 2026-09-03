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
    static std::unique_ptr<PBRMaterial> create_default();

    // Create metallic material
    static std::unique_ptr<PBRMaterial> create_metallic(const Vec3& color, float roughness = 0.1f);

    // Create dielectric material
    static std::unique_ptr<PBRMaterial> create_dielectric(const Vec3& color, float roughness = 0.5f);

    // Create emissive material
    static std::unique_ptr<PBRMaterial> create_emissive(const Vec3& color, float intensity = 1.0f);
};

} // namespace litt
