// Tests for Litt Engine C++ core
// Usage: g++ -std=c++17 -o tests tests.cpp && ./tests

#include "littcore/litt_math.h"
#include "littcore/litt_ecs.h"
#include "littcore/litt_input.h"
#include "littcore/litt_world.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace litt;

int tests_passed = 0;
int tests_failed = 0;

struct TestFail {};

#define TEST(name) void name()
#define RUN(name) do { \
    printf("  %-40s", #name); \
    try { name(); tests_passed++; printf(" PASS\n"); } \
    catch (const TestFail&) { tests_failed++; printf(" FAIL\n"); } \
} while(0)

#define ASSERT(expr) do { \
    if (!(expr)) { \
        printf("FAILED: %s at line %d\n", #expr, __LINE__); \
        throw TestFail{}; \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b) do { \
    float _a = (a), _b = (b); \
    if (std::abs(_a - _b) > 1e-5f) { \
        printf("FAILED: float eq %.6f == %.6f at line %d\n", _a, _b, __LINE__); \
        throw TestFail{}; \
    } \
} while(0)

// ============================================================================
// Math Tests
// ============================================================================

TEST(vec2_ops) {
    Vec2 a(1, 2), b(3, 4);
    ASSERT((a + b) == Vec2(4, 6));
    ASSERT((a - b) == Vec2(-2, -2));
    ASSERT((a * 2.0f) == Vec2(2, 4));
    ASSERT(a.dot(b) == 11.0f);
    ASSERT_FLOAT_EQ(a.length(), std::sqrt(5.0f));
}

TEST(vec3_ops) {
    Vec3 a(1, 2, 3), b(4, 5, 6);
    ASSERT((a + b) == Vec3(5, 7, 9));
    ASSERT((a - b) == Vec3(-3, -3, -3));
    ASSERT((a * 2.0f) == Vec3(2, 4, 6));
    ASSERT(a.dot(b) == 32.0f);
    ASSERT((a.cross(b)) == Vec3(-3, 6, -3));
    ASSERT_FLOAT_EQ(a.length(), std::sqrt(14.0f));
    ASSERT_FLOAT_EQ(a.normalized().length(), 1.0f);
}

TEST(mat4_identity) {
    Mat4 m = Mat4::identity();
    Vec3 v(1, 2, 3);
    ASSERT((m * v) == v);
}

TEST(mat4_translation) {
    Mat4 t = Mat4::translation(Vec3(1, 2, 3));
    Vec3 v = t * Vec3(0, 0, 0);
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    ASSERT_FLOAT_EQ(v.z, 3.0f);
}

TEST(mat4_perspective) {
    Mat4 p = Mat4::perspective(60.0f, 16.0f/9.0f, 0.1f, 100.0f);
    ASSERT(p.m[0] > 0);
    ASSERT(p.m[5] > 0);
    ASSERT(p.m[10] < 0);
}

TEST(mat4_lookat) {
    Vec3 eye(0, 5, -10), target(0, 0, 0), up(0, 1, 0);
    Mat4 m = Mat4::look_at(eye, target, up);
    // View matrix must map the eye position to the origin
    Vec3 t = m * eye;
    ASSERT_FLOAT_EQ(t.x, 0.0f);
    ASSERT_FLOAT_EQ(t.y, 0.0f);
    ASSERT_FLOAT_EQ(t.z, 0.0f);
}

TEST(mat4_lookat_orientation) {
    // Regression: look_at used to build its forward axis as (target - eye),
    // which mirrored X and pointed view-space +Z at the scene - everything in
    // front of the camera got negative clip w through perspective (m[11]=-1)
    // and was culled. GL convention: camera looks down -Z.
    Vec3 eye(0, 0, 10), target(0, 0, 0), up(0, 1, 0);
    Mat4 v = Mat4::look_at(eye, target, up);
    Vec3 p = v * Vec3(1, 0, 5);          // right of and in front of camera
    ASSERT(p.x > 0.0f);                  // world +X is screen-right
    ASSERT(p.z < 0.0f);                  // ahead of camera => negative z
    Mat4 pr = Mat4::perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    Vec4 c = pr * Vec4(p.x, p.y, p.z, 1.0f);
    ASSERT(c.w > 0.0f);                  // visible point has positive clip w
}

TEST(quat_identity) {
    Quat q = Quat::identity();
    Vec3 v(1, 0, 0);
    ASSERT((q.transform(v)) == v);
}

TEST(quat_rotation) {
    Quat q = Quat::from_axis_angle(Vec3::unit_y(), 3.14159f / 2.0f);
    Vec3 v = q.transform(Vec3::unit_z());
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 0.0f);
    ASSERT_FLOAT_EQ(v.z, 0.0f);
}

TEST(aabb_contains) {
    Aabb a({-1, -1, -1}, {1, 1, 1});
    ASSERT(a.contains(Vec3(0, 0, 0)));
    ASSERT(!a.contains(Vec3(2, 0, 0)));
    ASSERT(!a.contains(Vec3(0, 2, 0)));
    ASSERT(!a.contains(Vec3(0, 0, 2)));
}

TEST(aabb_intersects) {
    Aabb a({-1, -1, -1}, {1, 1, 1});
    Aabb b({0, 0, 0}, {2, 2, 2});
    Aabb c({5, 5, 5}, {6, 6, 6});
    ASSERT(a.intersects(b));
    ASSERT(!a.intersects(c));
}

TEST(ray_aabb) {
    Ray r(Vec3(0, 0, -5), Vec3(0, 0, 1));
    Aabb a({-1, -1, 0}, {1, 1, 2});
    HitInfo hit = ray_aabb(r, a);
    ASSERT(hit.hit);
    ASSERT_FLOAT_EQ(hit.t, 5.0f);
}

TEST(lerp) {
    Vec3 a(0, 0, 0), b(10, 10, 10);
    ASSERT_FLOAT_EQ(lerp(a, b, 0.0f).x, 0.0f);
    ASSERT_FLOAT_EQ(lerp(a, b, 0.5f).x, 5.0f);
    ASSERT_FLOAT_EQ(lerp(a, b, 1.0f).x, 10.0f);
}

TEST(clamp) {
    ASSERT(clamp(5.0f, 0.0f, 10.0f) == 5.0f);
    ASSERT(clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
    ASSERT(clamp(15.0f, 0.0f, 10.0f) == 10.0f);
}

// ============================================================================
// ECS Tests
// ============================================================================

TEST(entity_create) {
    World w;
    auto e = w.create();
    ASSERT(e.valid());
    ASSERT(w.is_alive(e));
}

TEST(entity_destroy) {
    World w;
    auto e = w.create();
    w.destroy(e);
    ASSERT(!w.is_alive(e));
}

TEST(component_add_get) {
    World w;
    auto e = w.create();
    auto& t = w.add<Transform>(e, Transform{Vec3(1, 2, 3)});
    ASSERT(t.position == Vec3(1, 2, 3));
    
    auto* t2 = w.get<Transform>(e);
    ASSERT(t2 != nullptr);
    ASSERT(t2->position == Vec3(1, 2, 3));
}

TEST(component_remove) {
    World w;
    auto e = w.create();
    w.add<Transform>(e, Transform{Vec3(1, 2, 3)});
    w.remove<Transform>(e);
    ASSERT(w.get<Transform>(e) == nullptr);
}

TEST(multiple_components) {
    World w;
    auto e = w.create();
    w.add<Transform>(e, Transform{Vec3(1, 2, 3)});
    w.add<Collider>(e, Collider{{Vec3(-1, -1, -1), Vec3(1, 1, 1)}});
    w.add<RigidBody>(e, RigidBody{});
    
    ASSERT(w.get<Transform>(e) != nullptr);
    ASSERT(w.get<Collider>(e) != nullptr);
    ASSERT(w.get<RigidBody>(e) != nullptr);
}

TEST(query) {
    World w;
    auto e1 = w.create();
    auto e2 = w.create();
    w.add<Transform>(e1, Transform{Vec3(1, 2, 3)});
    w.add<Transform>(e2, Transform{Vec3(4, 5, 6)});
    
    int count = 0;
    w.query<Transform>([&count](Entity, Transform*) { count++; });
    ASSERT(count == 2);
}

TEST(system_update) {
    World w;
    struct TestSystem : World::System {
        int calls = 0;
        void update(float dt) override { calls += (int)(dt * 100); }
    };
    auto sys = std::make_unique<TestSystem>();
    w.add_system(std::move(sys));

    w.update(0.1f);
    // System should have been called
}

TEST(component_swap_remove_stress) {
    // Regression: Storage::remove used the BACK ENTITY ID as a data index,
    // so after any remove+add cycle it moved from data[id] - out of bounds.
    World w;
    auto e0 = w.create(), e1 = w.create(), e2 = w.create();
    w.add<Transform>(e0, Transform{Vec3(0, 0, 0)});
    w.add<Transform>(e1, Transform{Vec3(1, 1, 1)});
    w.add<Transform>(e2, Transform{Vec3(2, 2, 2)});
    w.remove<Transform>(e1);                       // swap e2 into slot 1
    auto e3 = w.create();                          // id 3 >= data size 2
    w.add<Transform>(e3, Transform{Vec3(3, 3, 3)});
    w.remove<Transform>(e2);                       // old code: data[3] OOB move
    ASSERT(w.get<Transform>(e0)->position == Vec3(0, 0, 0));
    ASSERT(w.get<Transform>(e3)->position == Vec3(3, 3, 3));
    ASSERT(w.get<Transform>(e1) == nullptr);
    ASSERT(w.get<Transform>(e2) == nullptr);
    int count = 0;
    w.query<Transform>([&count](Entity, Transform* t) {
        ASSERT(t != nullptr);
        count++;
    });
    ASSERT(count == 2);
}

// ============================================================================
// Input Tests
// ============================================================================

TEST(input_key) {
    Input in;
    ASSERT(!in.key_down(Key::W));
    in.press(Key::W);
    ASSERT(in.key_down(Key::W));
    in.release(Key::W);
    ASSERT(!in.key_down(Key::W));
}

TEST(input_action) {
    Input in;
    in.load_defaults();
    in.bind("test", Key::Space);
    ASSERT(!in.action("test"));
    in.press(Key::Space);
    ASSERT(in.action("test"));
}

TEST(input_mouse) {
    Input in;
    ASSERT(!in.mouse_down(Mouse::Left));
    in.mouse_press(Mouse::Left);
    ASSERT(in.mouse_down(Mouse::Left));
    in.mouse_release(Mouse::Left);
    ASSERT(!in.mouse_down(Mouse::Left));
}

// ============================================================================
// Main
// ============================================================================

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("Litt Engine Tests\n");
    printf("==================\n\n");
    
    printf("[Math]\n");
    RUN(vec2_ops);
    RUN(vec3_ops);
    RUN(mat4_identity);
    RUN(mat4_translation);
    RUN(mat4_perspective);
    RUN(mat4_lookat);
    RUN(mat4_lookat_orientation);
    RUN(quat_identity);
    RUN(quat_rotation);
    RUN(aabb_contains);
    RUN(aabb_intersects);
    RUN(ray_aabb);
    RUN(lerp);
    RUN(clamp);
    
    printf("\n[ECS]\n");
    RUN(entity_create);
    RUN(entity_destroy);
    RUN(component_add_get);
    RUN(component_remove);
    RUN(multiple_components);
    RUN(query);
    RUN(system_update);
    RUN(component_swap_remove_stress);
    
    printf("\n[Input]\n");
    RUN(input_key);
    RUN(input_action);
    RUN(input_mouse);
    
    printf("\n==================\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
