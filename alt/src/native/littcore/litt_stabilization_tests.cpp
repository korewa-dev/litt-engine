// Litt Engine stabilization regression tests
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include "litt_math.h"
#include "litt_memory.h"
#include "litt_ecs.h"
#include "litt_physics.h"
#include "litt_scene.h"
#include "litt_gpu_software.h"

using namespace litt;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

static bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static PhysicsBody body_at(const Vec3& center, float half = 1.0f) {
    PhysicsBody body;
    body.centerOfMass = center;
    body.aabb = Aabb(center - Vec3(half, half, half),
                     center + Vec3(half, half, half));
    return body;
}

static float overlap_x(const PhysicsBody& a, const PhysicsBody& b) {
    return std::max(0.0f, std::min(a.aabb.max.x, b.aabb.max.x) -
                            std::max(a.aabb.min.x, b.aabb.min.x));
}

static void test_physics_normals() {
    NarrowPhase np;
    NarrowPhase::Contact c{};

    PhysicsBody left = body_at(Vec3(0, 0, 0));
    PhysicsBody right = body_at(Vec3(1.5f, 0, 0));
    check(np.testAabbAabb(left.aabb, right.aabb, c) && c.normal.x > 0.0f,
          "physics_normal_left_to_right");
    check(np.testAabbAabb(right.aabb, left.aabb, c) && c.normal.x < 0.0f,
          "physics_normal_right_to_left");

    PhysicsBody below = body_at(Vec3(0, 0, 0));
    PhysicsBody above = body_at(Vec3(0, 1.5f, 0));
    check(np.testAabbAabb(below.aabb, above.aabb, c) && c.normal.y > 0.0f,
          "physics_normal_below_to_above");
    check(np.testAabbAabb(above.aabb, below.aabb, c) && c.normal.y < 0.0f,
          "physics_normal_above_to_below");
}

static void test_physics_resolution() {
    {
        PhysicsBody a = body_at(Vec3(0, 0, 0));
        PhysicsBody b = body_at(Vec3(1.5f, 0, 0));
        const float before = overlap_x(a, b);
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&a);
        physics.addBody(&b);
        physics.update();
        check(overlap_x(a, b) < before, "physics_overlap_reduces");
        check(near(a.aabb.center().x, a.centerOfMass.x) &&
              near(b.aabb.center().x, b.centerOfMass.x),
              "physics_aabb_refresh");
    }

    {
        PhysicsBody fixed = body_at(Vec3(0, 0, 0));
        PhysicsBody dynamic = body_at(Vec3(1.5f, 0, 0));
        fixed.isStatic = true;
        fixed.inverseMass = 999.0f; // must be ignored
        const Vec3 fixed_before = fixed.centerOfMass;
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&fixed);
        physics.addBody(&dynamic);
        physics.update();
        check(fixed.centerOfMass == fixed_before, "physics_static_body_does_not_move");
        check(dynamic.centerOfMass.x > 1.5f, "physics_dynamic_moves_from_static");
    }

    {
        PhysicsBody trigger = body_at(Vec3(0, 0, 0));
        PhysicsBody other = body_at(Vec3(1.5f, 0, 0));
        trigger.isTrigger = true;
        const Vec3 a0 = trigger.centerOfMass, b0 = other.centerOfMass;
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&trigger);
        physics.addBody(&other);
        physics.update();
        check(trigger.centerOfMass == a0 && other.centerOfMass == b0,
              "physics_trigger_no_resolution");
        check(!physics.narrowPhase.contacts.empty(), "physics_trigger_reports_contact");
    }

    {
        PhysicsBody a = body_at(Vec3(0, 0, 0));
        PhysicsBody b = body_at(Vec3(1.5f, 0, 0));
        a.isStatic = b.isStatic = true;
        const Vec3 a0 = a.centerOfMass, b0 = b.centerOfMass;
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&a);
        physics.addBody(&b);
        physics.update();
        check(a.centerOfMass == a0 && b.centerOfMass == b0,
              "physics_both_static_unchanged");
    }

    {
        PhysicsBody a = body_at(Vec3(0, 0, 0));
        PhysicsBody b = body_at(Vec3(1.5f, 0, 0));
        a.velocity = Vec3(-1, 0, 0);
        b.velocity = Vec3(1, 0, 0);
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&a);
        physics.addBody(&b);
        physics.update();
        check(a.velocity.x < 0 && b.velocity.x > 0, "physics_moving_apart_no_impulse");
    }

    {
        PhysicsBody a = body_at(Vec3(0, 0, 0));
        PhysicsBody b = body_at(Vec3(1.5f, 0, 0));
        a.velocity = Vec3(1, 0, 0);
        b.velocity = Vec3(-1, 0, 0);
        PhysicsSystem physics(0.0f, Vec3::zero());
        physics.addBody(&a);
        physics.addBody(&b);
        physics.update();
        check((b.velocity - a.velocity).dot(Vec3::unit_x()) >= -1e-4f,
              "physics_moving_together_impulse");
    }
}

struct alignas(16) Aligned16 { unsigned char data[16]; };
struct alignas(64) Aligned64 { unsigned char data[64]; };
struct alignas(128) Aligned128 { unsigned char data[128]; };

template <typename T>
static bool pointer_aligned(T* ptr) {
    return reinterpret_cast<uintptr_t>(ptr) % alignof(T) == 0;
}

static void test_memory() {
    ObjectPool<Aligned16, 1> p16;
    ObjectPool<Aligned64, 1> p64;
    ObjectPool<Aligned128, 1> p128;
    auto* a16 = p16.acquire();
    auto* a64 = p64.acquire();
    auto* a128 = p128.acquire();
    check(pointer_aligned(a16), "object_pool_align16");
    check(pointer_aligned(a64), "object_pool_align64");
    check(pointer_aligned(a128), "object_pool_align128");
    p16.release(a16); p64.release(a64); p128.release(a128);

    BumpAllocator bump(256);
    void* b128 = bump.allocate(1, 128);
    check(reinterpret_cast<uintptr_t>(b128) % 128 == 0, "bump_absolute_alignment");

    bool invalid_alignment = false;
    try { (void)bump.allocate(1, 3); }
    catch (const std::invalid_argument&) { invalid_alignment = true; }
    check(invalid_alignment, "bump_rejects_non_power_two_alignment");

    ArenaAllocator arena(64);
    (void)arena.allocate(32, 16);
    auto cp = arena.checkpoint();
    void* expected = arena.allocate(16, 16);
    (void)arena.allocate(256, 16); // force a newer chunk
    arena.rollback(cp);
    void* reused = arena.allocate(16, 16);
    check(reused == expected, "arena_rollback_cross_chunk");

    FreeListAllocator free_list(9, 4);
    void* free_block = free_list.allocate();
    check(reinterpret_cast<uintptr_t>(free_block) % alignof(std::max_align_t) == 0,
          "freelist_stride_alignment");
    free_list.deallocate(free_block);
}

static void test_ecs_generations() {
    World world;
    Entity first = world.create();
    world.add<int>(first, 7);
    check(world.has<int>(first) && *world.get<int>(first) == 7, "ecs_component_add");

    world.destroy(first);
    check(!world.is_alive(first) && world.get<int>(first) == nullptr,
          "ecs_stale_handle_rejected");

    bool add_rejected = false;
    try { world.add<int>(first, 9); }
    catch (const std::invalid_argument&) { add_rejected = true; }
    check(add_rejected, "ecs_dead_add_rejected");

    Entity reused = world.create();
    check(reused.id == first.id && reused.gen != first.gen,
          "ecs_reuse_increments_generation");
    check(world.is_alive(reused) && !world.is_alive(first), "ecs_generation_liveness");
}

static void test_affine_inverse() {
    Mat4 transform = Mat4::translation(Vec3(3, -2, 5)) *
                     Mat4::rot_y(0.7f) *
                     Mat4::rot_x(-0.25f) *
                     Mat4::scale(Vec3(2.0f, 3.0f, 0.5f));
    Vec3 p(1.25f, -4.0f, 2.5f);
    Vec3 roundtrip = transform.affine_inverse() * (transform * p);
    check(near(roundtrip.x, p.x) && near(roundtrip.y, p.y) && near(roundtrip.z, p.z),
          "affine_inverse_nonuniform_trs");
}

static void test_software_renderer_hardening() {
    SoftwareRenderer renderer;
    check(renderer.initialize("headless"), "software_headless_initializes");
    renderer.clear(0x010203);
    const uint32_t before = renderer.get_pixel(400, 300);

    // These inputs previously reached unsafe Win32 calls, out-of-range vertex
    // reads, or non-terminating grid loops. They must be harmless in headless
    // fallback mode.
    renderer.draw_text(0, 0, "headless", 0xFFFFFF);
    renderer.draw_grid(0.0f, Mat4::identity(), 0xFFFFFF);
    renderer.draw_grid(-1.0f, Mat4::identity(), 0xFFFFFF);
    renderer.draw_grid(std::nanf(""), Mat4::identity(), 0xFFFFFF);
    renderer.draw_mesh({Vec3(0, 0, 0)}, {0, 1, 2}, Mat4::identity(), 0xFFFFFF);
    renderer.draw_pixel_depth(400, 300, std::nanf(""), 0xFFFFFF);
    renderer.draw_pixel_depth(400, 300, -1.0f, 0xFFFFFF);
    renderer.draw_pixel_depth(400, 300, 2.0f, 0xFFFFFF);

    check(renderer.get_pixel(400, 300) == before,
          "software_invalid_inputs_leave_framebuffer_unchanged");
    renderer.shutdown();
}

static void test_scene_lifecycle() {
    SceneManager manager;
    Scene& first = manager.createScene("Level");
    manager.setActiveScene("Level");
    Scene& duplicate = manager.createScene("Level");
    check(&first == &duplicate && manager.getActiveScene() == &first,
          "scene_duplicate_does_not_replace_active");

    check(first.serializeToJson().empty(), "scene_serialize_reports_unavailable");
    check(!first.deserializeFromJson("{}"), "scene_deserialize_reports_unavailable");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Stabilization]\n");
    test_physics_normals();
    test_physics_resolution();
    test_memory();
    test_ecs_generations();
    test_affine_inverse();
    test_software_renderer_hardening();
    test_scene_lifecycle();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
