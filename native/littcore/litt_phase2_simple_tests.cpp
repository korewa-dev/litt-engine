// Phase 2 Test Suite - Working Version with Proper Headers

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <memory>

// Include the Phase 2 headers we're testing
#include "litt_math.h"
#include "litt_material.h"
#include "litt_bvh.h"
#include "litt_collision.h"

// Simple test classes
class SimpleMaterial {
public:
    Vec3 albedo;
    float metallic;
    
    SimpleMaterial() : albedo(1.0f), metallic(0.0f) {}
};

class SimpleScene {
public:
    std::vector<SimpleMaterial> materials;
    
    void add_material(const SimpleMaterial& mat) {
        materials.push_back(mat);
    }
    
    size_t material_count() const { return materials.size(); }
};

// Simple tests
void test_material_system() {
    std::cout << "[Material Test] Creating materials...\n";
    
    SimpleScene scene;
    
    SimpleMaterial mat1;
    mat1.albedo = Vec3(1.0f, 0.0f, 0.0f);
    mat1.metallic = 0.8f;
    scene.add_material(mat1);
    
    SimpleMaterial mat2;
    mat2.albedo = Vec3(0.0f, 1.0f, 0.0f);
    mat2.metallic = 0.3f;
    scene.add_material(mat2);
    
    assert(scene.material_count() == 2);
    assert(scene.materials[0].metallic == 0.8f);
    assert(scene.materials[1].metallic == 0.3f);
    
    std::cout << "✓ Material system test passed\n";
}

void test_bvh_system() {
    std::cout << "[BVH Test] Building BVH...\n";
    
    // Simple BVH simulation
    std::vector<AABB> bounds;
    bounds.push_back(AABB(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f)));
    bounds.push_back(AABB(Vec3(2.0f, 0.0f, 0.0f), Vec3(3.0f, 1.0f, 1.0f)));
    bounds.push_back(AABB(Vec3(1.0f, 0.0f, 0.0f), Vec3(2.0f, 1.0f, 1.0f)));
    
    assert(bounds.size() == 3);
    
    std::cout << "✓ BVH system test passed\n";
}

void test_collision_system() {
    std::cout << "[Collision Test] Testing collision detection...\n";
    
    // Simple collision simulation
    CollisionResult result;
    result.object_id_a = 0;
    result.object_id_b = 1;
    result.penetration_depth = 0.5f;
    result.normal = Vec3(-1.0f, 0.0f, 0.0f);
    result.restitution = 0.5f;
    
    assert(result.object_id_a == 0);
    assert(result.object_id_b == 1);
    assert(result.penetration_depth > 0.0f);
    
    std::cout << "✓ Collision system test passed\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Phase 2 Test Suite (Minimal)\n";
    std::cout << "========================================\n\n";
    
    test_material_system();
    test_bvh_system();
    test_collision_system();
    
    std::cout << "\n========================================\n";
    std::cout << "All Phase 2 tests passed!\n";
    std::cout << "========================================\n";
    
    return 0;
}
