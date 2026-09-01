// Phase 3: Rendering Pipeline - Lighting System

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>

namespace litt {

// Light types
enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOT,
    AREA
};

// Light structure
struct Light {
    LightType type = LightType::DIRECTIONAL;
    Vec3 position = Vec3(0.0f);
    Vec3 direction = Vec3(0.0f, -1.0f, 0.0f);
    Vec3 color = Vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
    float spot_angle = 45.0f;
    float spot_penumbra = 0.5f;
    bool casts_shadows = false;
    uint32_t shadow_map_size = 1024;
};

// PBR Lighting calculations
class PBR Lighting {
public:
    // Calculate direct lighting contribution
    static Vec3 calculate_direct_light(const Light& light, 
                                      const Vec3& world_pos,
                                      const Vec3& normal,
                                      const Vec3& view_dir,
                                      const Vec3& albedo,
                                      float metallic,
                                      float roughness);
    
    // Calculate Cook-Torrance specular
    static float distribution_ggx(const Vec3& normal, const Vec3& half_vec, float roughness);
    static float geometry_schlick_ggx(float NdotV, float roughness);
    static float geometry_smith(const Vec3& normal, const Vec3& view_dir, const Vec3& light_dir, float roughness);
    static Vec3 fresnel_schlick(float cos_theta, Vec3 F0);
    static Vec3 fresnel_schlick_roughness(float cos_theta, Vec3 F0, float roughness);
    
    // Calculate irradiance from environment
    static Vec3 calculate_irradiance(const Vec3& normal, const Vec3& irradiance_map);
    
    // Image-based lighting
    static Vec3 ibl_specular(const Vec3& reflect_dir, float roughness, const Vec3& prefiltered_map);
    static Vec3 ibl_diffuse(const Vec3& normal, const Vec3& irradiance);
};

// Shadow mapping
class ShadowMap {
public:
    ShadowMap(uint32_t size = 1024);
    ~ShadowMap();
    
    // Begin shadow pass
    void begin_pass(const Vec3& light_pos, const Vec3& light_dir, float fov = 90.0f);
    
    // End shadow pass
    void end_pass();
    
    // Get depth matrix
    const Mat4& get_light_view_proj() const { return light_view_proj_; }
    
    // Sample shadow map
    float sample(float x, float y) const;
    
    // Get shadow map size
    uint32_t get_size() const { return size_; }

private:
    uint32_t size_;
    Mat4 light_view_proj_;
    std::vector<float> depth_data_;
};

// Light manager
class LightManager {
public:
    static LightManager& get_instance() {
        static LightManager instance;
        return instance;
    }
    
    // Add light to scene
    uint32_t add_light(const Light& light);
    
    // Remove light
    void remove_light(uint32_t id);
    
    // Get light
    Light* get_light(uint32_t id);
    
    // Get all lights
    const std::vector<std::unique_ptr<Light>>& get_lights() const { return lights_; }
    
    // Get lights affecting a point
    std::vector<Light*> get_lights_affecting_point(const Vec3& point, float radius) const;
    
    // Get number of lights
    size_t get_light_count() const { return lights_.size(); }
    
    // Clear all lights
    void clear();

private:
    LightManager() = default;
    uint32_t next_id_ = 1;
    std::vector<std::unique_ptr<Light>> lights_;
};

} // namespace litt
