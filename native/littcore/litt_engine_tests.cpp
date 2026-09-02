// =============================================================================
// Litt Engine - Complete Test Suite
// =============================================================================

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdio>

#include "litt_math.h"
#include "litt_engine_systems.h"

using namespace litt;

int passed = 0, failed = 0;

void check(bool cond, const char* name) {
    if (cond) { passed++; printf("  ok %s\n", name); }
    else { failed++; printf("  FAIL %s\n", name); }
}

int main() {
    printf("Starting tests...\n");
    
    printf("[Math]\n");
    Vec3 a(1,2,3), b(4,5,6);
    check((a+b).x == 5, "vec3_add");
    check(a.dot(b) == 32.0f, "vec3_dot");
    check(std::abs(a.length() - std::sqrt(14.0f)) < 0.01f, "vec3_length");
    printf("Math done\n");
    
    printf("\n[Radiometry]\n");
    check(schlick_fresnel(0.0f, 0.04f) == 1.0f, "fresnel_grazing");
    check(std::abs(schlick_fresnel(1.0f, 0.04f) - 0.04f) < 0.01f, "fresnel_normal");
    printf("Radiometry done\n");
    
    printf("\n[Path Tracing]\n");
    PT_Triangle tri;
    tri.v0 = Vec3(-1, -1, -5); tri.v1 = Vec3(1, -1, -5); tri.v2 = Vec3(0, 1, -5);
    tri.material.albedo = Vec3(0.8f, 0.8f, 0.8f);
    tri.precompute();
    check(std::abs(tri.normal.z - 1.0f) < 0.01f, "triangle_normal");
    
    Ray ray(Vec3(0, 0, 0), Vec3(0, 0, -1));
    float t_val, u, v;
    bool hit_result = ray_triangle_intersect(ray, tri, t_val, u, v);
    check(hit_result, "ray_triangle_hit");
    printf("Ray triangle done: t=%f\n", t_val);
    
    std::vector<PT_Triangle> tris = {tri};
    UnidirectionalPathTracer tracer;
    tracer.set_triangles(&tris);
    printf("About to build BVH...\n");
    auto& bvh = tracer.build_bvh();
    printf("BVH built: %s\n", bvh ? "yes" : "no");
    check(bvh != nullptr, "bvh_build");
    
    printf("About to trace...\n");
    Vec3 color = tracer.trace_path(ray, 0);
    printf("Path traced: %f %f %f\n", color.x, color.y, color.z);
    
    printf("\nDone! Results: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
