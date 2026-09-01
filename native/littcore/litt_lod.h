// Phase 4: Optimization & Performance - Level of Detail (LOD)

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>

namespace litt {

// LOD level
struct LODLevel {
    float distance_threshold; // Distance at which this LOD becomes active
    uint32_t index_count;
    uint32_t vertex_count;
    float quality; // 0.0 - 1.0 quality factor
};

// LOD group
class LODGroup {
public:
    LODGroup() = default;
    
    // Add LOD level
    void add_level(const LODLevel& level);
    
    // Get LOD level for distance
    uint32_t get_lod_for_distance(float distance) const;
    
    // Get LOD level by index
    const LODLevel* get_level(uint32_t index) const;
    
    // Get total levels
    uint32_t get_level_count() const { return levels_.size(); }
    
    // Calculate LOD factor (0.0 = highest detail, 1.0 = lowest)
    float get_lod_factor(float distance) const;

private:
    std::vector<LODLevel> levels_;
};

// LOD system
class LODSystem {
public:
    static LODSystem& get_instance() {
        static LODSystem instance;
        return instance;
    }
    
    // Create LOD group
    LODGroup* create_group(const std::string& name);
    
    // Get LOD group
    LODGroup* get_group(const std::string& name);
    
    // Remove LOD group
    void remove_group(const std::string& name);
    
    // Update LOD for camera position
    void update(const Vec3& camera_pos);
    
    // Set global LOD bias
    void set_lod_bias(float bias) { lod_bias_ = bias; }
    float get_lod_bias() const { return lod_bias_; }
    
    // Enable/disable LOD
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

private:
    LODSystem() = default;
    std::unordered_map<std::string, std::unique_ptr<LODGroup>> groups_;
    float lod_bias_ = 1.0f;
    bool enabled_ = true;
};

// LOD component for entities
struct LODComponent {
    LODGroup* group = nullptr;
    Vec3 world_position;
    uint32_t current_lod = 0;
    float distance_to_camera = 0.0f;
    
    void update_lod(const Vec3& camera_pos);
};

} // namespace litt
