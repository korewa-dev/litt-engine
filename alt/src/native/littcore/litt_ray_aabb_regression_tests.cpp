#include <cmath>
#include <cstdio>

#include "litt_math.h"

using namespace litt;

static bool nearf(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

static int check(bool condition, const char* name) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", name);
        return 1;
    }
    std::printf("ok %s\n", name);
    return 0;
}

int main() {
    int failed = 0;
    const Aabb box(Vec3{-1.0f, -1.0f, -1.0f}, Vec3{1.0f, 1.0f, 1.0f});

    const HitInfo outside = ray_aabb(
        Ray(Vec3{0.0f, 0.0f, -5.0f}, Vec3{0.0f, 0.0f, 1.0f}, 0.0f, 10.0f), box);
    failed += check(outside.hit && nearf(outside.t, 4.0f), "ray_aabb_outside_entry");

    const HitInfo inside = ray_aabb(
        Ray(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 0.0f, 10.0f), box);
    failed += check(inside.hit && nearf(inside.t, 1.0f), "ray_aabb_inside_exit");

    const HitInfo clipped_inside = ray_aabb(
        Ray(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 2.0f, 10.0f), box);
    failed += check(!clipped_inside.hit, "ray_aabb_clipped_inside_miss");

    const HitInfo parallel = ray_aabb(
        Ray(Vec3{2.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, 0.0f, 10.0f), box);
    failed += check(!parallel.hit, "ray_aabb_parallel_miss");

    return failed == 0 ? 0 : 1;
}
