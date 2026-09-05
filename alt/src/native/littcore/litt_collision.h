// Phase 2: Advanced Collision Detection System

#pragma once

#include "litt_math.h"
using namespace litt;

// Collision Query Types
enum class QueryType {
    RAY,
    SPHERE,
    Aabb,
    OBB,
    FRUSTUM
};

// Collision Filter - Controls which objects can collide
class CollisionFilter {
public:
    virtual ~CollisionFilter() = default;
    virtual bool should_collide(uint32_t object_id_a, uint32_t object_id_b) const {
        return true; // Default: all objects collide
    }
};

// Collision Result
struct CollisionResult {
    uint32_t object_id_a;
    uint32_t object_id_b;
    float penetration_depth;
    Vec3 normal;
    Vec3 contact_point;
    Vec3 relative_velocity;
    float restitution;
    float friction;
    
    CollisionResult() : object_id_a(UINT32_MAX), object_id_b(UINT32_MAX),
                       penetration_depth(0.0f), restitution(0.5f), friction(0.5f) {}
};

// Collision System - Main collision detection and response
class CollisionSystem {
public:
    CollisionSystem(std::shared_ptr<CollisionFilter> filter = nullptr);
    
    // Ray collision
    bool raycast(const Ray& ray, float max_distance, uint32_t& object_id,
                 Vec3& hit_point, Vec3& hit_normal, float& distance) const;
    
    // Sphere collision
    bool sphere_collision(uint32_t object_id, const Vec3& center, float radius,
                         CollisionResult& result) const;
    
    // Aabb collision
    bool aabb_collision(uint32_t object_id_a, const Aabb& bounds_a,
                       uint32_t object_id_b, const Aabb& bounds_b,
                       CollisionResult& result) const;
    
    // Sweep and prune broad phase
    void sweep_and_prune(const std::vector<Aabb>& bounds, 
                        std::vector<std::pair<uint32_t, uint32_t>>& pairs);
    
    // BVH broad phase
    bool bvh_collision(const std::vector<Aabb>& bounds,
                      uint32_t object_id_a, uint32_t object_id_b,
                      CollisionResult& result) const;
    
    // Multiple object collision detection
    void detect_collisions(std::vector<CollisionResult>& results);
    
    // Add object to collision system
    void add_object(uint32_t object_id, const Aabb& bounds);
    
    // Update object bounds
    void update_object_bounds(uint32_t object_id, const Aabb& bounds);
    
    // Remove object
    void remove_object(uint32_t object_id);
    
    // Set collision filter
    void set_collision_filter(std::shared_ptr<CollisionFilter> filter);
    
    // Get object bounds
    const Aabb& get_object_bounds(uint32_t object_id) const;
    
    // Check if object exists
    bool has_object(uint32_t object_id) const { return objects_.find(object_id) != objects_.end(); }

private:
    std::shared_ptr<CollisionFilter> filter_;
    std::unordered_map<uint32_t, Aabb> objects_;
    std::vector<Aabb> bounds_cache_;
    
    // Helper methods
    bool check_ray_aabb(const Ray& ray, const Aabb& bounds, 
                       float max_distance, Vec3& hit_point, 
                       Vec3& hit_normal, float& distance) const;
    
    bool check_sphere_aabb(const Vec3& sphere_center, float sphere_radius,
                          const Aabb& bounds, CollisionResult& result) const;
    
    bool check_aabb_aabb(const Aabb& bounds_a, const Aabb& bounds_b,
                        CollisionResult& result) const;
    
    void update_bounds_cache();
};

// Collision Resolution
class CollisionResolver {
public:
    // Position-based dynamics resolution
    static void resolve_position_based_dynamics(std::vector<CollisionResult>& collisions,
                                                std::vector<Vec3>& positions,
                                                std::vector<Vec3>& velocities,
                                                float dt, float restitution = 0.5f);
    
    // Velocity-based resolution
    static void resolve_velocity(std::vector<CollisionResult>& collisions,
                                 std::vector<Vec3>& velocities,
                                 float dt, float restitution = 0.5f,
                                 float friction = 0.5f);
    
    // Impulse resolution
    static void resolve_impulse(std::vector<CollisionResult>& collisions,
                               std::vector<Vec3>& velocities,
                               std::vector<float>& masses,
                               float dt, float restitution = 0.5f,
                               float friction = 0.5f);
    
    // Continuous collision detection (CCD)
    static bool continuous_collision_detection(const Vec3& p1, const Vec3& v1,
                                              const Vec3& p2, const Vec3& v2,
                                              float max_time, 
                                              CollisionResult& result);
    
    // Time of impact calculation
    static bool time_of_impact(const Ray& ray, float max_distance,
                              uint32_t object_id, float mass,
                              float& toi, Vec3& contact_point);
};

// Narrow Phase Collision Detection
class NarrowPhaseCollider {
public:
    // GJK (Gilbert-Johnson-Keerthi) collision detection
    static bool gjk_collision(const Vec3& A1, const Vec3& A2, const Vec3& A3,
                             const Vec3& B1, const Vec3& B2, const Vec3& B3,
                             CollisionResult& result);
    
    // EPA (Expanding Polytope Algorithm) for penetration depth
    static bool epa_penetration_depth(const std::vector<Vec3>& simplex,
                                     const Vec3& direction,
                                     float& depth, Vec3& normal);
    
    // Sphere-Sphere collision
    static bool sphere_sphere_collision(const Vec3& center_a, float radius_a,
                                       const Vec3& center_b, float radius_b,
                                       CollisionResult& result);
    
    // Aabb-OBB collision
    static bool aabb_obb_collision(const Aabb& aabb, const OBB& obb,
                                  CollisionResult& result);
    
    // Sphere-OBB collision
    static bool sphere_obb_collision(const Vec3& sphere_center, float sphere_radius,
                                    const OBB& obb, CollisionResult& result);
    
    // Capsule-Capsule collision
    static bool capsule_capsule_collision(const Vec3& p1_a, const Vec3& p2_a, float r_a,
                                         const Vec3& p1_b, const Vec3& p2_b, float r_b,
                                         CollisionResult& result);
    
    // Convex polygon collision (simplified)
    static bool convex_polygon_collision(const std::vector<Vec3>& poly_a,
                                        const std::vector<Vec3>& poly_b,
                                        CollisionResult& result);
    
    // Continuous collision for moving objects
    static bool continuous_sphere_collision(const Vec3& p1_a, const Vec3& v1_a, float r_a,
                                           const Vec3& p1_b, const Vec3& v1_b, float r_b,
                                           float max_time, CollisionResult& result);
    
    // Collision quality evaluator
    static float evaluate_collision_quality(const CollisionResult& result);
    
    // Collision point projection
    static Vec3 project_point_onto_plane(const Vec3& point, const Vec3& plane_normal,
                                         float plane_distance);
    
    // Velocity constraint application
    static Vec3 apply_velocity_constraint(const Vec3& velocity,
                                         const CollisionResult& collision,
                                         float dt);
};

// Object Collider - Wrapper for collision components
class ObjectCollider {
public:
    ObjectCollider(uint32_t id, const Aabb& bounds, float mass = 1.0f);
    
    // Get collision shape type
    enum class ShapeType {
        SPHERE,
        Aabb,
        OBB,
        CAPSULE
    };
    
    // Set shape
    void set_sphere(float radius);
    void set_aabb(const Aabb& bounds);
    void set_obb(const Mat4& transform, const Vec3& half_extents);
    void set_capsule(const Vec3& p1, const Vec3& p2, float radius);
    
    // Update collider
    void update(float dt);
    
    // Get shape type
    ShapeType get_shape_type() const { return shape_type_; }
    
    // Get collider bounds
    Aabb get_bounds() const;
    
    // Ray cast
    bool raycast(const Ray& ray, float max_distance, 
                Vec3& hit_point, Vec3& hit_normal, float& distance) const;
    
    // Collision detection
    bool collides_with(const ObjectCollider& other, CollisionResult& result) const;
    
    // Get object ID
    uint32_t get_id() const { return id_; }
    
    // Get mass
    float get_mass() const { return mass_; }
    
    // Set velocity
    void set_velocity(const Vec3& velocity) { velocity_ = velocity; }
    
    // Get velocity
    const Vec3& get_velocity() const { return velocity_; }
    
    // Get position
    const Vec3& get_position() const;
    
    // Set position
    void set_position(const Vec3& position);

private:
    uint32_t id_;
    ShapeType shape_type_ = ShapeType::Aabb;
    float mass_ = 1.0f;
    Vec3 position_ = Vec3::zero();
    Vec3 velocity_ = Vec3::zero();
    
    // Shape-specific data
    union ShapeData {
        struct {
            float radius;
        } sphere;
        
        struct {
            float half_extents[3];
        } box;
        
        struct {
            Vec3 center;
            float radius;
        } capsule;
        
        struct {
            Mat4 transform;
            Vec3 half_extents;
        } obb;
        
        ShapeData() {
            sphere.radius = 0.0f;
            box.half_extents[0] = 0.0f;
            box.half_extents[1] = 0.0f;
            box.half_extents[2] = 0.0f;
            capsule.center = Vec3::zero();
            capsule.radius = 0.0f;
            obb.transform = Mat4::identity();
            obb.half_extents = Vec3::zero();
        }
    } shape_data_;
    
    // Helper methods
    static Aabb compute_aabb_from_sphere(const Vec3& center, float radius);
    static Aabb compute_aabb_from_box(const Vec3& center, const Vec3& half_extents);
    static Aabb compute_aabb_from_capsule(const Vec3& p1, const Vec3& p2, float radius);
    static Aabb compute_aabb_from_obb(const Mat4& transform, const Vec3& half_extents);
};

// Collision Scene - Manages all colliders in a scene
class CollisionScene {
public:
    CollisionScene();
    
    // Add collider to scene
    void add_collider(std::unique_ptr<ObjectCollider> collider);
    
    // Remove collider from scene
    void remove_collider(uint32_t id);
    
    // Update scene (continuous collision detection)
    void update(float dt);
    
    // Detect all collisions in scene
    void detect_collisions(std::vector<CollisionResult>& results);
    
    // Resolve all collisions in scene
    void resolve_collisions(std::vector<CollisionResult>& results, float dt);
    
    // Get collider by ID
    ObjectCollider* get_collider(uint32_t id);
    
    // Get all colliders
    const std::vector<std::unique_ptr<ObjectCollider>>& get_colliders() const { return colliders_; }
    
    // Get collision system
    CollisionSystem& get_collision_system() { return collision_system_; }
    
    // Clear scene
    void clear();
    
    // Performance statistics
    struct PerformanceStats {
        double collision_detection_time_ms = 0.0;
        double collision_resolution_time_ms = 0.0;
        double broad_phase_time_ms = 0.0;
        double narrow_phase_time_ms = 0.0;
        uint32_t collisions_detected = 0;
        uint32_t collisions_resolved = 0;
        double avg_collision_time_ms = 0.0;
    };
    
    PerformanceStats get_performance_stats() const { return stats_; }

private:
    std::vector<std::unique_ptr<ObjectCollider>> colliders_;
    CollisionSystem collision_system_;
    PerformanceStats stats_;
    float fixed_dt_ = 1.0f / 60.0f; // Fixed time step
    
    void update_performance_stats(double detection_time, double resolution_time);
};