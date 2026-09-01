// Phase 2 Test Suite - Working Version

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <memory>

// Use the litt namespace to avoid prefixing
using namespace litt;

// Include the Phase 2 headers we're testing
#include "litt_material.h"
#include "litt_rasterizer.h"
#include "litt_bvh.h"
#include "litt_collision.h"

// Simple tests using the actual Phase 2 systems
void test_material_system() {
    std::cout << "[Material Test] Testing PBR Material System...\n";
    
    // Create a simple PBR material using the actual system
    PBRMaterial material;
    material.albedo = Vec3(1.0f, 0.0f, 0.0f);
    material.metallic = 0.8f;
    material.roughness = 0.2f;
    material.ao = 0.9f;
    
    // Test material properties
    assert(material.albedo.x == 1.0f);
    assert(material.metallic == 0.8f);
    assert(material.roughness == 0.2f);
    assert(material.ao == 0.9f);
    
    // Test helper methods
    float metal_rough = material.get_metal_roughness();
    assert(fabs(metal_rough - 0.16f) < 0.01f); // 0.8 * 0.2 = 0.16
    
    std::cout << "✓ PBR Material system test passed\n";
}

void test_rasterizer_system() {
    std::cout << "[Rasterizer Test] Testing Rasterizer Pipeline...\n";
    
    // Create a simple rasterizer
    Rasterizer rasterizer;
    
    // Test basic properties
    int x, y, width, height;
    rasterizer.get_viewport(x, y, width, height);
    assert(width == 800);
    assert(height == 600);
    
    // Test clearing
    rasterizer.clear(Vec3(0.5f, 0.5f, 0.5f));
    
    std::cout << "✓ Rasterizer system test passed\n";
}

void test_bvh_system() {
    std::cout << "[BVH Test] Testing BVH Acceleration Structure...\n";
    
    // Create a simple BVH scene
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
    
    // Build BVH
    scene.build_bvh(10, false); // Use simple version
    
    assert(scene.is_built());
    
    // Test ray intersection
    Ray test_ray;
    test_ray.origin = Vec3(-1.0f, 0.0f, 0.0f);
    test_ray.direction = Vec3(1.0f, 0.0f, 0.0f);
    
    float t;
    Vec3 normal;
    uint32_t id;
    bool hit = scene.intersect_scene(test_ray, t, normal, id);
    
    assert(hit); // Should hit the first primitive
    
    std::cout << "✓ BVH system test passed\n";
}

void test_collision_system() {
    std::cout << "[Collision Test] Testing Collision Detection...\n";
    
    // Create collision scene
    CollisionScene scene;
    
    // Add two colliders
    auto* collider1 = new ObjectCollider(0, AABB(Vec3(0.0f), Vec3(1.0f)), 1.0f);
    collider1->set_aabb(AABB(Vec3(0.0f), Vec3(1.0f)));
    scene.add_collider(std::unique_ptr<ObjectCollider>(collider1));
    
    auto* collider2 = new ObjectCollider(1, AABB(Vec3(1.5f), Vec3(2.5f)), 1.0f);
    collider2->set_aabb(AABB(Vec3(1.5f), Vec3(2.5f)));
    scene.add_collider(std::unique_ptr<ObjectCollider>(collider2));
    
    // Test collision detection
    std::vector<CollisionResult> results;
    scene.detect_collisions(results);
    
    // Should have detected collision (objects are close)
    assert(!results.empty());
    
    std::cout << "✓ Collision system test passed\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Phase 2 Test Suite (Complete)\n";
    std::cout << "========================================\n\n";
    
    test_material_system();
    test_rasterizer_system();
    test_bvh_system();
    test_collision_system();
    
    std::cout << "\n========================================\n";
    std::cout << "All Phase 2 tests passed!\n";
    std::cout << "Material System ✓\n";
    std::cout << "Rasterizer Pipeline ✓\n";
    std::cout << "BVH Acceleration ✓\n";
    std::cout << "Collision Detection ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
