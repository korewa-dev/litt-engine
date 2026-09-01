// Phase 2: Comprehensive Test Suite - Material, Rasterizer, BVH, Collision

#pragma once

#include "litt_math.h"
#include "litt_material.h"
#include "litt_rasterizer.h"
#include "litt_bvh.h"
#include "litt_collision.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <random>

// Test Utilities
class TestUtils {
public:
    static float random_float(float min = 0.0f, float max = 1.0f) {
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng);
    }
    
    static Vec3 random_vec3(float min = 0.0f, float max = 1.0f) {
        return Vec3(random_float(min, max), random_float(min, max), random_float(min, max));
    }
    
    static Mat4 random_transform() {
        Mat4 result = Mat4::identity();
        // Random translation, rotation, scale
        Vec3 translation = random_vec3(-10.0f, 10.0f);
        Vec3 scale = random_vec3(0.5f, 2.0f);
        
        // Build transform
        result = Mat4::translation(translation) * 
                Mat4::scale(scale);
        
        return result;
    }
    
    static AABB compute_aabb(const std::vector<Vec3>& points) {
        if (points.empty()) return AABB();
        
        Vec3 min = points[0];
        Vec3 max = points[0];
        
        for (const auto& point : points) {
            min = Vec3(std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z));
            max = Vec3(std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z));
        }
        
        return AABB(min, max);
    }
    
    static float distance_squared(const Vec3& a, const Vec3& b) {
        Vec3 diff = b - a;
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    }
    
    static float distance(const Vec3& a, const Vec3& b) {
        return sqrtf(distance_squared(a, b));
    }
    
    static Vec3 random_vector_in_unit_sphere() {
        Vec3 result;
        do {
            result = random_vec3(-1.0f, 1.0f);
        } while (result.length_squared() > 1.0f);
        return result;
    }
    
    static Ray random_ray_from_camera(const Vec3& origin, const Mat4& view_proj) {
        Vec3 random_point = random_vector_in_unit_sphere();
        Vec3 random_screen = Vec3(random_point.x, random_point.y, 0.0f);
        return Ray(origin, random_point.normalized());
    }
};

// PBR Material Tests
class PBRMaterialTests {
public:
    static void test_material_creation() {
        std::cout << "[Material - Creation]\n";
        
        PBRMaterial material;
        material.albedo = Vec3(1.0f, 0.0f, 0.0f);
        material.metallic = 0.8f;
        material.roughness = 0.2f;
        material.ao = 0.9f;
        material.emission = Vec3(0.1f, 0.1f, 0.1f);
        material.opacity = 1.0f;
        
        assert(material.albedo.x == 1.0f);
        assert(material.metallic == 0.8f);
        assert(material.roughness == 0.2f);
        assert(material.ao == 0.9f);
        
        std::cout << "  ✓ PASS: material_creation\n";
    }
    
    static void test_material_properties() {
        std::cout << "[Material - Properties]\n";
        
        PBRMaterial material;
        material.albedo = Vec3(0.5f, 0.5f, 0.5f);
        material.metallic = 0.3f;
        material.roughness = 0.7f;
        material.ao = 0.8f;
        
        // Test property getters
        float metal_rough = material.get_metal_roughness();
        assert(fabs(metal_rough - 0.21f) < 0.01f);
        
        Vec3 base_color = material.get_base_color();
        assert(fabs(base_color.x - 0.5f) < 0.01f);
        
        std::cout << "  ✓ PASS: material_properties\n";
    }
    
    static void test_material_manager() {
        std::cout << "[Material - Manager]\n";
        
        auto& manager = MaterialManager::get_instance();
        
        // Clear existing materials for test
        // Note: This would require a clear method in real implementation
        
        PBRMaterial material1;
        material1.albedo = Vec3(1.0f, 0.0f, 0.0f);
        material1.metallic = 0.5f;
        
        uint32_t id1 = manager.create_material("red_metallic", material1);
        assert(id1 != UINT32_MAX);
        
        PBRMaterial* retrieved1 = manager.get_material(id1);
        assert(retrieved1 != nullptr);
        assert(retrieved1->metallic == 0.5f);
        
        uint32_t id2 = manager.create_material("blue_matte", material1);
        assert(id2 != id1); // Different IDs
        
        PBRMaterial* retrieved2 = manager.get_material("blue_matte");
        assert(retrieved2 != nullptr);
        
        std::cout << "  ✓ PASS: material_manager\n";
    }
    
    static void test_material_serialization() {
        std::cout << "[Material - Serialization]\n";
        
        PBRMaterial original;
        original.albedo = Vec3(0.3f, 0.6f, 0.9f);
        original.metallic = 0.7f;
        original.roughness = 0.4f;
        original.ao = 0.8f;
        original.emission = Vec3(0.1f, 0.1f, 0.1f);
        original.opacity = 0.9f;
        original.albedo_map = "albedo.png";
        original.metallic_roughness_map = "mr.png";
        
        SerializedMaterial serialized = MaterialSerializer::to_serialized(original);
        
        PBRMaterial deserialized = MaterialSerializer::from_serialized(serialized);
        
        assert(deserialized.albedo.x == original.albedo.x);
        assert(deserialized.metallic == original.metallic);
        assert(deserialized.roughness == original.roughness);
        assert(deserialized.ao == original.ao);
        assert(deserialized.emission.x == original.emission.x);
        assert(deserialized.opacity == original.opacity);
        assert(deserialized.albedo_map == "albedo.png");
        
        std::cout << "  ✓ PASS: material_serialization\n";
    }
    
    static void test_pbr_brdf() {
        std::cout << "[PBR - BRDF]\n";
        
        PBRMaterial material;
        material.metallic = 0.5f;
        material.roughness = 0.3f;
        
        // Test BRDF functions
        float diffuse = PBRBRDF::lambertian(1.0f, 1.0f);
        assert(diffuse > 0.0f && diffuse < 1.0f);
        
        float ggx = PBRBRDF::ggx_distribution(0.5f, 0.3f);
        assert(ggx > 0.0f && ggx < 1.0f);
        
        float fresnel = PBRBRDF::fresnel_schlick(0.5f, 0.04f);
        assert(fresnel >= 0.04f && fresnel <= 1.0f);
        
        // Test PBR reflection calculation
        Vec3 L = Vec3(0.0f, 0.0f, 1.0f);
        Vec3 V = Vec3(0.0f, 0.0f, 1.0f);
        Vec3 N = Vec3(0.0f, 0.0f, 1.0f);
        Vec3 H = Vec3(0.0f, 0.0f, 1.0f);
        
        float reflection = PBRBRDF::pbr_reflection(L, V, N, H, material);
        assert(reflection >= 0.0f && reflection <= 1.0f);
        
        std::cout << "  ✓ PASS: pbr_brdf\n";
    }
    
    static void run_all_material_tests() {
        std::cout << "========================================\n";
        std::cout << "PBR Material Tests\n";
        std::cout << "========================================\n\n";
        
        test_material_creation();
        test_material_properties();
        test_material_manager();
        test_material_serialization();
        test_pbr_brdf();
        
        std::cout << "\n========================================\n";
        std::cout << "Material Tests: 5 passed, 0 failed\n";
        std::cout << "========================================\n\n";
    }
};

// Rasterizer Tests
class RasterizerTests {
public:
    static void test_rasterizer_creation() {
        std::cout << "[Rasterizer - Creation]\n";
        
        RasterState state;
        state.cull_mode = RasterState::CullMode::NONE;
        state.depth_test = false;
        
        Rasterizer rasterizer(state);
        
        // Test viewport
        int x, y, width, height;
        rasterizer.get_viewport(x, y, width, height);
        assert(width == 800 && height == 600);
        
        // Test clear
        rasterizer.clear(Vec3(1.0f, 0.0f, 0.0f));
        
        std::cout << "  ✓ PASS: rasterizer_creation\n";
    }
    
    static void test_rasterizer_triangles() {
        std::cout << "[Rasterizer - Triangles]\n";
        
        Rasterizer rasterizer;
        
        // Simple triangle test
        Vec3 v1(0.0f, 0.0f, 0.0f);
        Vec3 v2(1.0f, 0.0f, 0.0f);
        Vec3 v3(0.5f, 1.0f, 0.0f);
        Vec3 n1(0.0f, 0.0f, 1.0f);
        Vec3 n2(0.0f, 0.0f, 1.0f);
        Vec3 n3(0.0f, 0.0f, 1.0f);
        
        rasterizer.render_triangle(v1, v2, v3, n1, n2, n3, Vec3(1.0f, 0.0f, 0.0f));
        
        const auto& pixels = rasterizer.get_pixels();
        assert(pixels.size() > 0); // Should have rendered pixels
        
        std::cout << "  ✓ PASS: rasterizer_triangles\n";
    }
    
    static void test_rasterizer_viewport() {
        std::cout << "[Rasterizer - Viewport]\n";
        
        Rasterizer rasterizer;
        
        // Set custom viewport
        rasterizer.set_viewport(100, 100, 600, 400);
        
        int x, y, width, height;
        rasterizer.get_viewport(x, y, width, height);
        
        assert(x == 100);
        assert(y == 100);
        assert(width == 600);
        assert(height == 400);
        
        std::cout << "  ✓ PASS: rasterizer_viewport\n";
    }
    
    static void test_raster_utils() {
        std::cout << "[Rasterizer - Utils]\n";
        
        // Test line rasterization
        std::vector<Vec3> test_pixels;
        RasterUtils::rasterize_line(0, 0, 10, 10, test_pixels, Vec3(1.0f));
        assert(test_pixels.size() > 0);
        
        // Test circle rasterization
        std::vector<Vec3> circle_pixels;
        RasterUtils::rasterize_circle(10, 10, 5, circle_pixels, Vec3(0.0f, 1.0f, 0.0f));
        assert(circle_pixels.size() > 0);
        
        // Test point-in-triangle
        Vec3 A(0.0f, 0.0f, 0.0f);
        Vec3 B(1.0f, 0.0f, 0.0f);
        Vec3 C(0.5f, 1.0f, 0.0f);
        Vec3 P(0.5f, 0.5f, 0.0f);
        
        bool inside = RasterUtils::point_in_triangle(P, A, B, C);
        assert(inside);
        
        std::cout << "  ✓ PASS: raster_utils\n";
    }
    
    static void run_all_rasterizer_tests() {
        std::cout << "========================================\n";
        std::cout << "Rasterizer Tests\n";
        std::cout << "========================================\n\n";
        
        test_rasterizer_creation();
        test_rasterizer_triangles();
        test_rasterizer_viewport();
        test_raster_utils();
        
        std::cout << "\n========================================\n";
        std::cout << "Rasterizer Tests: 4 passed, 0 failed\n";
        std::cout << "========================================\n\n";
    }
};

// BVH Tests
class BVHTests {
public:
    static void test_bvh_creation() {
        std::cout << "[BVH - Creation]\n";
        
        BVHScene scene;
        
        // Add some primitives
        BVHPrimitive p1;
        p1.id = 0;
        p1.bounds = AABB(Vec3(0.0f), Vec3(1.0f));
        scene.add_primitive(p1);
        
        BVHPrimitive p2;
        p2.id = 1;
        p2.bounds = AABB(Vec3(2.0f), Vec3(3.0f));
        scene.add_primitive(p2);
        
        assert(scene.get_primitive_count() == 2);
        assert(!scene.is_built()); // Should not be built until build_bvh is called
        
        // Build BVH
        scene.build_bvh(10, false); // Use simple version
        
        assert(scene.is_built());
        assert(scene.get_node_count() > 0);
        
        std::cout << "  ✓ PASS: bvh_creation\n";
    }
    
    static void test_bvh_ray_intersection() {
        std::cout << "[BVH - Ray Intersection]\n";
        
        BVHScene scene;
        
        // Add a primitive
        BVHPrimitive p;
        p.id = 0;
        p.bounds = AABB(Vec3(0.0f), Vec3(1.0f));
        scene.add_primitive(p);
        
        scene.build_bvh(10, false);
        
        // Test ray that should hit
        Ray ray1;
        ray1.origin = Vec3(-1.0f, 0.0f, 0.0f);
        ray1.direction = Vec3(1.0f, 0.0f, 0.0f);
        
        float t1;
        Vec3 normal1;
        uint32_t id1;
        bool hit1 = scene.intersect_scene(ray1, t1, normal1, id1);
        
        // Test ray that should not hit
        Ray ray2;
        ray2.origin = Vec3(10.0f, 0.0f, 0.0f);
        ray2.direction = Vec3(1.0f, 0.0f, 0.0f);
        
        float t2;
        Vec3 normal2;
        uint32_t id2;
        bool hit2 = scene.intersect_scene(ray2, t2, normal2, id2);
        
        assert(hit1);
        assert(!hit2);
        
        std::cout << "  ✓ PASS: bvh_ray_intersection\n";
    }
    
    static void test_bvh_performance() {
        std::cout << "[BVH - Performance]\n";
        
        BVHScene scene;
        
        // Add many primitives
        for (int i = 0; i < 100; i++) {
            BVHPrimitive p;
            p.id = i;
            p.bounds = AABB(Vec3(i * 0.1f), Vec3(i * 0.1f + 0.05f));
            scene.add_primitive(p);
        }
        
        // Build BVH
        auto start_time = std::chrono::high_resolution_clock::now();
        scene.build_bvh(10, true); // Use SAH
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        assert(duration.count() > 0); // Should take some time to build
        
        // Test performance analyzer
        std::vector<Ray> rays;
        for (int i = 0; i < 10; i++) {
            Ray ray;
            ray.origin = TestUtils::random_vec3();
            ray.direction = TestUtils::random_vector_in_unit_sphere().normalized();
            rays.push_back(ray);
        }
        
        auto stats = BVHPerformanceAnalyzer::analyze_performance(scene, rays);
        assert(stats.nodes_visited > 0);
        
        std::cout << "  ✓ PASS: bvh_performance\n";
    }
    
    static void run_all_bvh_tests() {
        std::cout << "========================================\n";
        std::cout << "BVH Tests\n";
        std::cout << "========================================\n\n";
        
        test_bvh_creation();
        test_bvh_ray_intersection();
        test_bvh_performance();
        
        std::cout << "\n========================================\n";
        std::cout << "BVH Tests: 3 passed, 0 failed\n";
        std::cout << "========================================\n\n";
    }
};

// Collision Tests
class CollisionTests {
public:
    static void test_collision_system() {
        std::cout << "[Collision - System]\n";
        
        CollisionScene scene;
        
        // Add some colliders
        auto* box1 = new ObjectCollider(0, AABB(Vec3(0.0f), Vec3(1.0f)), 1.0f);
        box1->set_aabb(AABB(Vec3(0.0f), Vec3(1.0f)));
        scene.add_collider(std::unique_ptr<ObjectCollider>(box1));
        
        auto* box2 = new ObjectCollider(1, AABB(Vec3(2.0f), Vec3(3.0f)), 1.0f);
        box2->set_aabb(AABB(Vec3(2.0f), Vec3(3.0f)));
        scene.add_collider(std::unique_ptr<ObjectCollider>(box2));
        
        assert(scene.get_collider(0) != nullptr);
        assert(scene.get_collider(1) != nullptr);
        
        // Test collision detection
        std::vector<CollisionResult> results;
        scene.detect_collisions(results);
        
        // Boxes are separated, so no collision
        assert(results.empty());
        
        std::cout << "  ✓ PASS: collision_system\n";
    }
    
    static void test_collision_resolution() {
        std::cout << "[Collision - Resolution]\n";
        
        std::vector<Vec3> positions;
        positions.push_back(Vec3(0.0f, 0.0f, 0.0f));
        positions.push_back(Vec3(1.0f, 0.0f, 0.0f));
        
        std::vector<Vec3> velocities;
        velocities.push_back(Vec3(0.0f, 0.0f, 0.0f));
        velocities.push_back(Vec3(-1.0f, 0.0f, 0.0f));
        
        std::vector<float> masses;
        masses.push_back(1.0f);
        masses.push_back(1.0f);
        
        // Create collision result for overlapping objects
        CollisionResult result;
        result.object_id_a = 0;
        result.object_id_b = 1;
        result.penetration_depth = 0.5f;
        result.normal = Vec3(-1.0f, 0.0f, 0.0f);
        result.restitution = 0.5f;
        result.friction = 0.5f;
        
        std::vector<CollisionResult> collisions = {result};
        
        CollisionResolver::resolve_position_based_dynamics(collisions, positions, velocities, 0.016f);
        
        // Objects should have moved apart
        assert(positions[0].x > 0.0f || positions[1].x < 1.0f);
        
        std::cout << "  ✓ PASS: collision_resolution\n";
    }
    
    static void test_narrow_phase() {
        std::cout << "[Collision - Narrow Phase]\n";
        
        // Test sphere-sphere collision
        CollisionResult sphere_result;
        bool sphere_hit = NarrowPhaseCollider::sphere_sphere_collision(
            Vec3(0.0f), 1.0f, Vec3(2.0f), 1.0f, sphere_result);
        
        assert(!sphere_hit); // Spheres are separated
        
        // Test overlapping spheres
        CollisionResult sphere_overlap;
        bool sphere_overlap_hit = NarrowPhaseCollider::sphere_sphere_collision(
            Vec3(0.0f), 1.0f, Vec3(1.5f), 1.0f, sphere_overlap);
        
        assert(sphere_overlap_hit);
        assert(sphere_overlap.penetration_depth > 0.0f);
        
        // Test triangle collision
        std::vector<Vec3> triangle = {
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(1.0f, 0.0f, 0.0f),
            Vec3(0.5f, 1.0f, 0.0f)
        };
        
        CollisionResult triangle_result;
        bool triangle_hit = NarrowPhaseCollider::convex_polygon_collision(triangle, triangle, triangle_result);
        
        // Self-collision test
        assert(triangle_hit);
        
        std::cout << "  ✓ PASS: narrow_phase\n";
    }
    
    static void run_all_collision_tests() {
        std::cout << "========================================\n";
        std::cout << "Collision Tests\n";
        std::cout << "========================================\n\n";
        
        test_collision_system();
        test_collision_resolution();
        test_narrow_phase();
        
        std::cout << "\n========================================\n";
        std::cout << "Collision Tests: 3 passed, 0 failed\n";
        std::cout << "========================================\n\n";
    }
};

// Main Phase 2 Test Suite
int main_phase2_tests() {
    std::cout << "========================================\n";
    std::cout << "Litt Engine - Phase 2 Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Run all test suites
    PBRMaterialTests::run_all_material_tests();
    RasterizerTests::run_all_rasterizer_tests();
    BVHTests::run_all_bvh_tests();
    CollisionTests::run_all_collision_tests();
    
    std::cout << "========================================\n";
    std::cout << "Phase 2 Tests: All completed successfully!\n";
    std::cout << "========================================\n";
    
    return 0;
}
