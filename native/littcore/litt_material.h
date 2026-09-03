// Phase 2: Engine Architecture - Material System (PBR Implementation)

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <cmath>

namespace litt {

constexpr float PI = 3.14159265358979323846f;
inline float mix(float a, float b, float t) { return a * (1.0f - t) + b * t; }

// PBR Material Structure - Physically Based Rendering
struct PBRMaterial {
    // Base Color (Albedo)
    Vec3 albedo = Vec3(0.8f, 0.8f, 0.8f);
    float metallic = 0.0f;          // 0.0 = dielectric, 1.0 = metallic
    float roughness = 0.5f;         // 0.0 = smooth, 1.0 = rough
    float ao = 1.0f;                // Ambient occlusion (0.0-1.0)
    Vec3 emission = Vec3::zero();     // Self-illumination
    float opacity = 1.0f;           // Transparency (0.0 = transparent, 1.0 = opaque)
    
    // Textures (file paths for now)
    std::string albedo_map;
    std::string normal_map;
    std::string metallic_roughness_map;
    std::string ao_map;
    std::string emission_map;
    
    // PBR workflow helpers
    Vec3 get_base_color() const { return albedo; }
    float get_metal_roughness() const { return metallic * roughness; }
};

// BRDF (Bidirectional Reflectance Distribution Function) - Physically Based
class PBRBRDF {
public:
    // Lambertian diffuse model
    static float lambertian(float NdotL, float NdotV) {
        return NdotL / PI;
    }
    
    // Cook-Torrance specular model
    static float cook_torrance(float NdotL, float NdotV, float NdotH, 
                              float roughness, float metallic) {
        // Simplify for testing - full implementation would include
        // distribution, geometry, and Fresnel terms
        float alpha = roughness * roughness;
        float denom = NdotH * NdotH * (alpha - 1) + 1;
        return denom * denom / (4 * PI * alpha * alpha);
    }
    
    // GGX microfacet distribution
    static float ggx_distribution(float NdotH, float roughness) {
        float alpha = roughness * roughness;
        float denom = NdotH * NdotH * (alpha - 1) + 1;
        return alpha / (PI * denom * denom);
    }
    
    // Schlick Fresnel approximation
    static float fresnel_schlick(float cos_theta, float F0) {
        return F0 + (1.0f - F0) * pow(1.0f - cos_theta, 5);
    }
    
    // PBR total reflection calculation
    static float pbr_reflection(const Vec3& L, const Vec3& V, const Vec3& N,
                               const Vec3& H, const PBRMaterial& mat) {
        float NdotL = std::max(0.0f, dot(N, L));
        float NdotV = std::max(0.0f, dot(N, V));
        float NdotH = std::max(0.0f, dot(N, H));
        float LdotH = std::max(0.0f, dot(L, H));
        
        // Diffuse component
        float diffuse = lambertian(NdotL, NdotV);
        
        // Specular component
        float F0 = mix(0.04f, mat.albedo.length(), mat.metallic);
        float specular = cook_torrance(NdotL, NdotV, NdotH, mat.roughness, mat.metallic);
        
        // Energy conservation
        float total = diffuse * (1.0f - F0) + specular * F0;
        return total;
    }
};

// Material Manager - Handles material storage and lookup
class MaterialManager {
public:
    static MaterialManager& get_instance() {
        static MaterialManager instance;
        return instance;
    }
    
    uint32_t create_material(const std::string& name, const PBRMaterial& material) {
        uint32_t id = next_id_++;
        materials_[id] = std::make_unique<PBRMaterial>(material);
        name_map_[name] = id;
        return id;
    }
    
    PBRMaterial* get_material(uint32_t id) {
        auto it = materials_.find(id);
        return it != materials_.end() ? it->second.get() : nullptr;
    }
    
    PBRMaterial* get_material(const std::string& name) {
        auto it = name_map_.find(name);
        return it != name_map_.end() ? get_material(it->second) : nullptr;
    }
    
    const std::unordered_map<uint32_t, std::unique_ptr<PBRMaterial>>& get_all_materials() const {
        return materials_;
    }
    
    size_t material_count() const { return materials_.size(); }

private:
    MaterialManager() = default;
    uint32_t next_id_ = 1;
    std::unordered_map<uint32_t, std::unique_ptr<PBRMaterial>> materials_;
    std::unordered_map<std::string, uint32_t> name_map_;
};

// Material Sampling Interface - For texture sampling
class IMaterialSampler {
public:
    virtual ~IMaterialSampler() = default;
    virtual Vec3 sample_albedo(float u, float v) const = 0;
    virtual Vec3 sample_normal(float u, float v) const = 0;
    virtual float sample_metallic(float u, float v) const = 0;
    virtual float sample_roughness(float u, float v) const = 0;
    virtual float sample_ao(float u, float v) const = 0;
    virtual Vec3 sample_emission(float u, float v) const = 0;
};

// Simple Texture Sampler Implementation
class SimpleTextureSampler : public IMaterialSampler {
public:
    SimpleTextureSampler(const PBRMaterial& material) : material_(material) {}
    
    Vec3 sample_albedo(float u, float v) const override {
        // In real implementation, sample from albedo_map
        return material_.albedo;
    }
    
    Vec3 sample_normal(float u, float v) const override {
        // In real implementation, sample from normal_map
        return Vec3(0.5f, 0.5f, 1.0f); // Default normal
    }
    
    float sample_metallic(float u, float v) const override {
        // In real implementation, sample from metallic_roughness_map
        return material_.metallic;
    }
    
    float sample_roughness(float u, float v) const override {
        // In real implementation, sample from metallic_roughness_map
        return material_.roughness;
    }
    
    float sample_ao(float u, float v) const override {
        // In real implementation, sample from ao_map
        return material_.ao;
    }
    
    Vec3 sample_emission(float u, float v) const override {
        // In real implementation, sample from emission_map
        return material_.emission;
    }

private:
    const PBRMaterial& material_;
};

// Material Serialization - For saving/loading materials
struct SerializedMaterial {
    std::string name;
    Vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    Vec3 emission;
    float opacity;
    std::string albedo_map;
    std::string normal_map;
    std::string metallic_roughness_map;
    std::string ao_map;
    std::string emission_map;
};

class MaterialSerializer {
public:
    static SerializedMaterial to_serialized(const PBRMaterial& material) {
        SerializedMaterial sm;
        sm.albedo = material.albedo;
        sm.metallic = material.metallic;
        sm.roughness = material.roughness;
        sm.ao = material.ao;
        sm.emission = material.emission;
        sm.opacity = material.opacity;
        sm.albedo_map = material.albedo_map;
        sm.normal_map = material.normal_map;
        sm.metallic_roughness_map = material.metallic_roughness_map;
        sm.ao_map = material.ao_map;
        sm.emission_map = material.emission_map;
        return sm;
    }
    
    static PBRMaterial from_serialized(const SerializedMaterial& sm) {
        PBRMaterial mat;
        mat.albedo = sm.albedo;
        mat.metallic = sm.metallic;
        mat.roughness = sm.roughness;
        mat.ao = sm.ao;
        mat.emission = sm.emission;
        mat.opacity = sm.opacity;
        mat.albedo_map = sm.albedo_map;
        mat.normal_map = sm.normal_map;
        mat.metallic_roughness_map = sm.metallic_roughness_map;
        mat.ao_map = sm.ao_map;
        mat.emission_map = sm.emission_map;
        return mat;
    }
};

} // namespace litt
