// Phase 2: BVH (Bounding Volume Hierarchy) Implementation

#pragma once

#include "litt_math.h"
using namespace litt;

// BVH Primitive (Triangle for now, extensible)
struct BVHPrimitive {
    uint32_t id;
    AABB bounds;
    // Additional primitive data (normal, material, texcoord)
};

// BVH Node
class BVHNode {
public:
    AABB bounds;
    std::unique_ptr<BVHNode> left;
    std::unique_ptr<BVHNode> right;
    uint32_t primitive_index; // UINT32_MAX for internal nodes
    
    BVHNode() : primitive_index(UINT32_MAX) {}
    ~BVHNode() = default;
    
    bool is_leaf() const { return left == nullptr && right == nullptr; }
};

// BVH Builder - Constructs BVH from primitives
class BVHBuilder {
public:
    // SAH (Surface Area Heuristic) BVH construction
    static std::unique_ptr<BVHNode> build_sah(const std::vector<BVHPrimitive>& primitives,
                                              int max_depth = 10,
                                              int max_primitives_per_leaf = 4);
    
    // Simple bounding box BVH construction
    static std::unique_ptr<BVHNode> build_simple(const std::vector<BVHPrimitive>& primitives,
                                                 int max_depth = 10);
    
    // Compute cost using Surface Area Heuristic
    static float compute_sah_cost(const std::vector<BVHPrimitive>& primitives,
                                  int split_axis, float split_pos);
    
    // Find best split for SAH
    static std::pair<int, float> find_best_split_sah(const std::vector<BVHPrimitive>& primitives,
                                                     int axis);
    
    // Assign primitives to buckets for SAH
    static void assign_to_buckets(const std::vector<BVHPrimitive>& primitives,
                                 int axis, float split_pos,
                                 std::vector<std::vector<BVHPrimitive>>& buckets);
    
    // Compute bounds of primitives
    static AABB compute_bounds(const std::vector<BVHPrimitive>& primitives);
};

// BVH Ray Intersect - Fast ray tracing through BVH
class BVHRayIntersect {
public:
    // Ray-BVH intersection
    static bool intersect(const BVHNode* node, const Ray& ray, 
                         float& t, Vec3& normal, uint32_t& primitive_id,
                         float t_max = FLT_MAX);
    
    // Ray-AABB intersection test
    static bool intersect_aabb(const AABB& bounds, const Ray& ray, 
                              float& t, float t_max = FLT_MAX);
    
    // Intersect triangle with ray
    static bool intersect_triangle(const Vec3& A, const Vec3& B, const Vec3& C,
                                  const Vec3& normal, const Ray& ray,
                                  float& t, Vec3& barycentric, float t_max = FLT_MAX);
    
    // BVH traversal
    static bool traverse_bvh(const BVHNode* node, const Ray& ray,
                            float& t, Vec3& normal, uint32_t& primitive_id,
                            float t_max = FLT_MAX);
    
    // Intersect scene with BVH
    static void intersect_scene(const BVHNode* root, const Ray& ray,
                               std::vector<float>& hits, std::vector<Vec3>& normals,
                               std::vector<uint32_t>& primitive_ids,
                               float t_max = FLT_MAX);
    
    // Intersect sphere with ray (test primitive)
    static bool intersect_sphere(const Vec3& center, float radius, const Ray& ray,
                                float& t, Vec3& normal, float t_max = FLT_MAX);
    
    // Intersect box with ray (test primitive)
    static bool intersect_box(const Vec3& min, const Vec3& max, const Ray& ray,
                             float& t, Vec3& normal, float t_max = FLT_MAX);
    
    // Progress tracking for BVH
    static void traverse_with_progress(const BVHNode* node, const Ray& ray,
                                      float& t, Vec3& normal, uint32_t& primitive_id,
                                      float t_max = FLT_MAX,
                                      int& node_visits = node_visits_);
    
    static int get_node_visits() { return node_visits_; }
    static void reset_node_visits() { node_visits_ = 0; }

private:
    static int node_visits_;
};

// BVH Scene - Wrapper for BVH with scene management
class BVHScene {
public:
    BVHScene();
    
    // Add primitive to scene
    void add_primitive(const BVHPrimitive& primitive);
    
    // Build BVH from current primitives
    void build_bvh(int max_depth = 10, bool use_sah = true);
    
    // Clear scene
    void clear();
    
    // Get root node
    const BVHNode* get_root() const { return root_.get(); }
    
    // Get primitive by index
    const BVHPrimitive* get_primitive(uint32_t id) const;
    
    // Get all primitives
    const std::vector<BVHPrimitive>& get_primitives() const { return primitives_; }
    
    // Ray intersect scene
    bool intersect_scene(const Ray& ray, float& t, Vec3& normal, 
                       uint32_t& primitive_id, float t_max = FLT_MAX) {
        if (!root_) return false;
        return BVHRayIntersect::intersect(root_.get(), ray, t, normal, primitive_id, t_max);
    }
    
    // Get number of primitives
    size_t get_primitive_count() const { return primitives_.size(); }
    
    // Get number of BVH nodes
    size_t get_node_count() const { return node_count_; }
    
    // Check if BVH is built
    bool is_built() const { return root_ != nullptr; }

private:
    std::vector<BVHPrimitive> primitives_;
    std::unique_ptr<BVHNode> root_;
    size_t node_count_ = 0;
    bool use_sah_ = true;
};

// BVH Performance Analyzer
class BVHPerformanceAnalyzer {
public:
    struct PerformanceStats {
        double build_time_ms = 0.0;
        double traversal_time_ms = 0.0;
        double intersection_time_ms = 0.0;
        double sah_cost = 0.0;
        size_t nodes_visited = 0;
        double traversal_efficiency = 0.0;
    };
    
    static PerformanceStats analyze_performance(const BVHScene& scene, 
                                                const std::vector<Ray>& rays);
    
    // Compare BVH with naive implementation
    static void compare_with_naive(const BVHScene& scene, 
                                   const std::vector<Ray>& rays);
    
    // Get memory usage statistics
    static size_t get_memory_usage(const BVHNode* node);
    
    // Optimize BVH parameters
    static std::pair<int, bool> optimize_bvh_params(const BVHScene& scene,
                                                    const std::vector<Ray>& rays);

private:
    static double current_time();
};