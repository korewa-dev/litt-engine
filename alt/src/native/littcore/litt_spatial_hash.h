// Phase 4: Optimization & Performance - Spatial Hash Grid

#pragma once

#include "litt_math.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace litt {

// Spatial hash key
struct SpatialHashKey {
    int32_t x, y, z;
    
    bool operator==(const SpatialHashKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    struct Hash {
        size_t operator()(const SpatialHashKey& key) const {
            size_t h1 = std::hash<int32_t>()(key.x);
            size_t h2 = std::hash<int32_t>()(key.y);
            size_t h3 = std::hash<int32_t>()(key.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
};

// Spatial hash grid
class SpatialHashGrid {
public:
    SpatialHashGrid(float cell_size = 1.0f);
    ~SpatialHashGrid();
    
    // Set cell size
    void set_cell_size(float size) { cell_size_ = size; }
    float get_cell_size() const { return cell_size_; }
    
    // Insert object
    void insert(uint32_t object_id, const Vec3& position);
    
    // Insert object with radius
    void insert_sphere(uint32_t object_id, const Vec3& center, float radius);
    
    // Remove object
    void remove(uint32_t object_id);
    
    // Update object position
    void update(uint32_t object_id, const Vec3& position);
    
    // Query objects in sphere
    std::vector<uint32_t> query_sphere(const Vec3& center, float radius) const;
    
    // Query objects in AABB
    std::vector<uint32_t> query_aabb(const AABB& aabb) const;
    
    // Query nearest neighbors
    std::vector<uint32_t> query_nearest(const Vec3& point, uint32_t max_count) const;
    
    // Get object count
    size_t get_object_count() const { return object_positions_.size(); }
    
    // Clear all objects
    void clear();
    
    // Get stats
    uint32_t get_cell_count() const { return static_cast<uint32_t>(cells_.size()); }

private:
    SpatialHashKey get_cell_key(const Vec3& position) const;
    Vec3 get_cell_center(const SpatialHashKey& key) const;
    
    float cell_size_;
    std::unordered_map<SpatialHashKey, std::unordered_set<uint32_t>, SpatialHashKey::Hash> cells_;
    std::unordered_map<uint32_t, Vec3> object_positions_;
};

// Spatial hash for collision broad-phase
class SpatialHashCollision {
public:
    SpatialHashCollision(float cell_size = 1.0f);
    
    // Insert collider
    void insert(uint32_t object_id, const AABB& bounds);
    
    // Query potential collisions
    std::vector<std::pair<uint32_t, uint32_t>> query_collision_pairs() const;
    
    // Query potential collisions for object
    std::vector<uint32_t> query_potential_collisions(uint32_t object_id) const;
    
    // Clear
    void clear();

private:
    SpatialHashGrid grid_;
};

} // namespace litt
