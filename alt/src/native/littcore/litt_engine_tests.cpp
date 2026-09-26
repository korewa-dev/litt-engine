// Litt Engine - canonical module contract tests
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "litt_engine_systems.h"

using namespace litt;

static int passed = 0;
static int failed = 0;

static void check(bool condition, const char* name) {
    if (condition) {
        ++passed;
        std::printf("  ok %s\n", name);
    } else {
        ++failed;
        std::printf("  FAIL %s\n", name);
    }
}

static bool nearf(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

static void test_umbrella() {
    check(EngineSystemsContract::version == 1u, "systems_umbrella_version");
    Vec3 v = Mat4::identity() * Vec3(1.0f, 2.0f, 3.0f);
    check(v.x == 1.0f && v.y == 2.0f && v.z == 3.0f, "math_through_umbrella");
}

static void test_math_rays() {
    const Aabb box(Vec3{-1.0f, -1.0f, -1.0f}, Vec3{1.0f, 1.0f, 1.0f});

    const HitInfo outside = ray_aabb(
        Ray(Vec3{-2.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 0.0f, 10.0f),
        box);
    check(outside.hit && nearf(outside.t, 1.0f), "ray_aabb_outside_entry");

    const HitInfo inside = ray_aabb(
        Ray(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 0.0f, 10.0f),
        box);
    check(inside.hit && nearf(inside.t, 1.0f) &&
          nearf(inside.point.x, 1.0f),
          "ray_aabb_inside_returns_exit");

    const HitInfo clipped = ray_aabb(
        Ray(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 0.0f, 0.5f),
        box);
    check(!clipped.hit, "ray_aabb_inside_exit_beyond_tmax_misses");

    const HitInfo parallel_miss = ray_aabb(
        Ray(Vec3{2.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, 0.0f, 10.0f),
        box);
    check(!parallel_miss.hit, "ray_aabb_parallel_outside_slab_misses");
}

static void test_input() {
    Input input;
    input.load_defaults();
    input.press(Key::Space);
    input.update();
    check(input.action("jump"), "input_bound_action");
    check(input.action_pressed("jump"), "input_press_edge");
    input.update();
    check(!input.action_pressed("jump"), "input_edge_not_repeated");
    input.release(Key::Space);
    input.update();
    check(!input.action("jump"), "input_release");

    input.mouse_move(10.0, 5.0);
    input.mouse_move(13.0, 9.0);
    input.scroll(2.0);
    input.scroll(-0.5);
    input.update();
    const auto [dx, dy] = input.mouse_delta();
    const auto [mx, my] = input.mouse_pos();
    check(dx == 13.0 && dy == 9.0, "input_mouse_delta_accumulates_for_frame");
    check(mx == 13.0 && my == 9.0, "input_mouse_position_tracks_latest_event");
    check(input.scroll() == 1.5, "input_scroll_accumulates_for_frame");

    input.update();
    const auto [next_dx, next_dy] = input.mouse_delta();
    check(next_dx == 0.0 && next_dy == 0.0 && input.scroll() == 0.0,
          "input_pointer_edges_reset_next_frame");
}

static void test_ecs_system_boundary() {
    World world;
    world.add_system(nullptr);
    world.update(1.0f / 60.0f);
    check(true, "ecs_null_system_is_harmless");
}

static void test_ui() {
    UIManager ui;
    int clicks = 0;
    auto window = std::make_shared<UIWindow>("main", Vec2{0, 0}, Vec2{200, 100});
    auto button = std::make_shared<UIButton>("go", Vec2{10, 10}, Vec2{80, 30});
    button->onClick([&clicks] { ++clicks; });
    window->addElement(button);
    ui.addWindow(window);
    ui.onMouseDown(Vec2{20, 20});
    check(clicks == 1 && button->isPressed(), "ui_button_click");
    button->onMouseUp(Vec2{20, 20});
    check(!button->isPressed(), "ui_button_release");

    UISlider slider(Vec2{0, 0}, Vec2{100, 10}, -1.0f, 1.0f);
    slider.onMouseDown(Vec2{75, 5});
    check(nearf(slider.getValue(), 0.5f), "ui_slider_position");
}

static void test_physics() {
    PhysicsBody a;
    a.centerOfMass = Vec3{0, 0, 0};
    a.aabb = Aabb(Vec3{-1, -1, -1}, Vec3{1, 1, 1});
    a.inverseMass = 1.0f;

    PhysicsBody b;
    b.centerOfMass = Vec3{1.5f, 0, 0};
    b.aabb = Aabb(Vec3{0.5f, -1, -1}, Vec3{2.5f, 1, 1});
    b.inverseMass = 0.0f;
    b.isStatic = true;

    PhysicsSystem physics(1.0f / 60.0f, Vec3::zero());
    check(physics.addBody(&a) && physics.addBody(&b), "physics_add_valid_bodies");
    physics.update();
    check(a.centerOfMass.x < 0.0f, "physics_resolves_dynamic_static_overlap");

    PhysicsBody invalid = a;
    invalid.centerOfMass.x = std::nanf("");
    check(!physics.addBody(&invalid), "physics_rejects_nonfinite_body");
}

static std::shared_ptr<AudioClip> make_clip() {
    auto clip = std::make_shared<AudioClip>();
    clip->sampleRate = 8000;
    clip->channels = 1;
    clip->lengthSamples = 4;
    clip->data = {0.25f, -0.5f, 1.0f, -1.0f};
    return clip;
}

static void test_audio() {
    SoftwareAudioMixer mixer;
    check(mixer.initialize(8000), "audio_mixer_init");
    check(mixer.add_source("tone", make_clip()), "audio_add_source");
    check(mixer.play("tone"), "audio_play");
    float frames[8]{};
    check(mixer.render(frames, 4), "audio_render");
    check(nearf(frames[0], 0.25f) && nearf(frames[1], 0.25f), "audio_mono_to_stereo");
}

static void test_lod() {
    LODSystem& lod = LODSystem::get_instance();
    lod.clear();
    LODGroup* group = lod.create_group("world");
    check(group != nullptr, "lod_group_create");
    check(group && group->add_level({10.0f, 300, 100, 1.0f}), "lod_level_near");
    check(group && group->add_level({50.0f, 120, 40, 0.5f}), "lod_level_mid");
    check(group && group->add_level({200.0f, 30, 10, 0.2f}), "lod_level_far");

    LODComponent component;
    component.group = group;
    component.world_position = Vec3{25, 0, 0};
    check(lod.register_component(&component), "lod_component_register");
    lod.update(Vec3::zero());
    check(component.current_lod == 1u, "lod_distance_selection");
    check(lod.set_lod_bias(2.0f), "lod_bias_set");
    lod.update(Vec3::zero());
    check(component.current_lod == 2u, "lod_bias_applied");
    check(lod.unregister_component(&component), "lod_component_unregister");
    lod.clear();
}

static void test_serialization() {
    JSONSerializer json;
    json.set_data("{\"name\":\"litt\",\"version\":1}");
    const auto json_bytes = json.serialize_to_buffer();
    check(!json_bytes.empty(), "json_serialize_valid");

    JSONSerializer restored;
    check(restored.deserialize_from_buffer(json_bytes), "json_deserialize_valid");
    check(restored.get_data() == json.get_data(), "json_roundtrip");
    check(!restored.deserialize_from_buffer(std::vector<uint8_t>{'{','x','}'}),
          "json_rejects_invalid");

    BinarySerializer binary;
    check(binary.write_uint(42u) && binary.write_float(3.5f) &&
          binary.write_string("engine") && binary.write_bool(true),
          "binary_write");
    const auto bytes = binary.serialize_to_buffer();
    BinarySerializer decoded;
    check(decoded.deserialize_from_buffer(bytes), "binary_deserialize");
    check(decoded.read_uint() == 42u && nearf(decoded.read_float(), 3.5f) &&
          decoded.read_string() == "engine" && decoded.read_bool() && decoded.good(),
          "binary_roundtrip");
    (void)decoded.read_uint();
    check(!decoded.good(), "binary_bounds_failure_is_sticky");
}

static void test_profiler() {
    Profiler& profiler = Profiler::get_instance();
    profiler.reset();
    profiler.begin_sample("contract");
    profiler.end_sample("contract");
    const ProfilerStats stats = profiler.get_stats("contract");
    check(stats.sample_count == 1u && stats.total_time_ms >= 0.0, "profiler_sample");
    profiler.update_fps();
    profiler.update_fps();
    check(profiler.get_frame_time_ms() >= 0.0, "profiler_frame_time");
}

static void test_software_renderer() {
    SoftwareRenderer renderer;
    check(renderer.set_framebuffer_size(160, 120), "software_renderer_size");
    check(renderer.initialize("headless"), "software_renderer_init");
    renderer.clear(0);
    renderer.draw_triangle(10, 10, 50, 10, 30, 50, 0x00ff00u);
    check(renderer.get_pixel(30, 20) == 0x00ff00u, "software_renderer_triangle");
    renderer.shutdown();

    Renderer facade;
    check(facade.initialize(160, 120, RenderBackend::Software),
          "renderer_facade_software_init");
    facade.clear(Vec3{1.0f, 0.0f, 0.0f});
    check(facade.get_width() == 160u && facade.get_height() == 120u &&
          facade.get_pixel(0, 0) == 0xff0000u,
          "renderer_facade_clear_and_dimensions");
    facade.shutdown();
    check(!facade.initialize(160, 120, RenderBackend::Vulkan) &&
          !facade.initialized(),
          "renderer_facade_rejects_unavailable_backend");

    EngineConfig config;
    check(config.backend == RenderBackend::Software, "engine_default_renderer_supported");
}

int main() {
    test_umbrella();
    test_math_rays();
    test_input();
    test_ecs_system_boundary();
    test_ui();
    test_physics();
    test_audio();
    test_lod();
    test_serialization();
    test_profiler();
    test_software_renderer();

    std::printf("Results: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
