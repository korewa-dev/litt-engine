// Phase 4: Optimization & Performance - Culling System

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>

namespace litt {

// Frustum for view frustum culling
struct Frustum {
    struct Plane {
        Vec3 normal;
        float distance;
        
        float signed_distance(const Vec3& point) const {
            return normal.dot(point) + distance;
        }
    };
    
    Plane planes[6]; // near, far, left, right, top, bottom
    
    // Extract frustum from view-projection matrix
    static Frustum from_matrix(const Mat4& view_proj);
    
    // Test if point is inside frustum
    bool contains_point(const Vec3& point) const;
    
    // Test if sphere is inside frustum
    bool contains_sphere(const Vec3& center, float radius) const;
    
    // Test if AABB is inside frustum
    bool contains_aabb(const AABB& aabb) const;
    
    // Test if AABB intersects frustum (partial visibility)
    bool intersects_aabb(const AABB& aabb) const;
};

// Occlusion culling using software rasterization
class OcclusionCuller {
public:
    OcclusionCuller(uint32_t width = 256, uint32_t height = 256);
    ~OcclusionCuller();
    
    // Begin occlusion frame
    void begin_frame(const Mat4& view_proj);
    
    // Render occluder (depth-only)
    void render_occluder(const Vec3* vertices, uint32_t count);
    
    // Test if AABB is occluded
    bool is_occluded(const AABB& aabb) const;
    
    // Test if sphere is occluded
    bool is_occluded_sphere(const Vec3& center, float radius) const;
    
    // End frame and reset depth buffer
    void end_frame();

private:
    uint32_t width_;
    uint32_t height_;
    std::vector<float> depth_buffer_;
    Mat4 view_proj_;
};

// Culling system
class CullingSystem {
public:
    static CullingSystem& get_instance() {
        static CullingSystem instance;
        return instance;
    }
    
    // Set camera frustum
    void set_frustum(const Frustum& frustum);
    
    // Set occlusion culler
    void set_occlusion_culler(std::shared_ptr<OcclusionCuller> culler);
    
    // Perform frustum culling
    void frustum_cull(const std::vector<AABB>& objects, 
                      std::vector<uint32_t>& visible_indices);
    
    // Perform occlusion culling
    void occlusion_cull(const std::vector<AABB>& objects,
                        std::vector<uint32_t>& visible_indices);
    
    // Combined culling (frustum + occlusion)
    void cull(const std::vector<AABB>& objects,
              std::vector<uint32_t>& visible_indices);
    
    // Get stats
    uint32_t get_culled_count() const { return culled_count_; }
    uint32_t get_visible_count() const { return visible_count_; }

private:
    CullingSystem() = default;
    Frustum frustum_;
    std::shared_ptr<OcclusionCuller> occlusion_culler_;
    uint32_t culled_count_ = 0;
    uint32_t visible_count_ = 0;
};

} // namespace litt
