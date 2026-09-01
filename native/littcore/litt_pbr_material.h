// Phase 3: Rendering Pipeline - Material System Extension (PBR + Textures)

#pragma once

#include "litt_math.h"
#include "litt_texture.h"
#include <string>
#include <memory>

namespace litt {

// Material property block
struct MaterialProperty {
    std::string name;
    enum class Type { FLOAT, INT, VEC2, VEC3, VEC4, TEXTURE } type;
    
    union {
        float float_value;
        int int_value;
    };
    Vec4 vec4_value;
    Texture* texture_value = nullptr;
};

// PBR Material with texture support
class PBRMaterial {
public:
    PBRMaterial();
    ~PBRMaterial();
    
    // Set base color
    void set_albedo(const Vec3& color) { albedo_ = color; }
    const Vec3& get_albedo() const { return albedo_; }
    
    // Set metallic
    void set_metallic(float metallic) { metallic_ = metallic; }
    float get_metallic() const { return metallic_; }
    
    // Set roughness
    void set_roughness(float roughness) { roughness_ = roughness; }
    float get_roughness() const { return roughness_; }
    
    // Set ambient occlusion
    void set_ao(float ao) { ao_ = ao; }
    float get_ao() const { return ao_; }
    
    // Set emission
    void set_emission(const Vec3& emission) { emission_ = emission; }
    const Vec3& get_emission() const { return emission_; }
    
    // Set textures
    void set_albedo_map(Texture* texture) { albedo_map_ = texture; }
    void set_normal_map(Texture* texture) { normal_map_ = texture; }
    void set_metallic_roughness_map(Texture* texture) { metallic_roughness_map_ = texture; }
    void set_ao_map(Texture* texture) { ao_map_ = texture; }
    void set_emission_map(Texture* texture) { emission_map_ = texture; }
    
    // Get textures
    Texture* get_albedo_map() const { return albedo_map_; }
    Texture* get_normal_map() const { return normal_map_; }
    Texture* get_metallic_roughness_map() const { return metallic_roughness_map_; }
    Texture* get_ao_map() const { return ao_map_; }
    Texture* get_emission_map() const { return emission_map_; }
    
    // Bind material
    void bind() const;
    
    // Unbind material
    void unbind() const;

private:
    Vec3 albedo_ = Vec3(0.8f);
    float metallic_ = 0.0f;
    float roughness_ = 0.5f;
    float ao_ = 1.0f;
    Vec3 emission_ = Vec3(0.0f);
    
    Texture* albedo_map_ = nullptr;
    Texture* normal_map_ = nullptr;
    Texture* metallic_roughness_map_ = nullptr;
    Texture* ao_map_ = nullptr;
    Texture* emission_map_ = nullptr;
};

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
