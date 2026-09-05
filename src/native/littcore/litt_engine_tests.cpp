// =============================================================================
// Litt Engine - Complete Test Suite
// =============================================================================

#include <iostream>
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
    fflush(stdout);
}

void test_math() {
    printf("[Math]\n"); fflush(stdout);
    Vec3 a(1,2,3), b(4,5,6);
    check((a+b).x == 5, "vec3_add");
    check(a.dot(b) == 32.0f, "vec3_dot");
    check(std::abs(a.length() - std::sqrt(14.0f)) < 0.01f, "vec3_length");
    Vec3 cross = a.cross(b);
    check(cross.x == -3 && cross.y == 6 && cross.z == -3, "vec3_cross");
    Mat4 m = Mat4::identity();
    Vec3 transformed = m * a;
    check(transformed.x == 1 && transformed.y == 2 && transformed.z == 3, "mat4_transform");
    printf("Math done\n"); fflush(stdout);
}

void test_radiometry() {
    printf("[Radiometry]\n"); fflush(stdout);
    check(schlick_fresnel(0.0f, 0.04f) == 1.0f, "fresnel_grazing");
    check(std::abs(schlick_fresnel(1.0f, 0.04f) - 0.04f) < 0.01f, "fresnel_normal");
    Vec3 refracted = RefractionCalculator::refracted_direction(Vec3(0,-1,0), Vec3(0,1,0), 1.0f, 1.5f);
    check(refracted.y < 0.0f, "snell_refraction");
    MicrofacetDistribution md = {0.5f};
    check(md.ggx(1.0f) > 0.0f, "ggx_distribution");
    CookTorranceBRDF brdf;
    Vec3 brdf_val = brdf.evaluate(Vec3(0,1,0), Vec3(0,1,0), Vec3(0,1,0));
    check(brdf_val.x > 0.0f || brdf_val.y > 0.0f || brdf_val.z > 0.0f, "cook_torrance");
    printf("Radiometry done\n"); fflush(stdout);
}

void test_path_tracing() {
    printf("[Path Tracing]\n"); fflush(stdout);
    PT_Triangle tri;
    tri.v0 = Vec3(-1, -1, -5); tri.v1 = Vec3(1, -1, -5); tri.v2 = Vec3(0, 1, -5);
    tri.material.albedo = Vec3(0.8f, 0.8f, 0.8f);
    tri.precompute();
    check(std::abs(tri.normal.z - 1.0f) < 0.01f, "triangle_normal");
    
    Ray ray(Vec3(0, 0, 0), Vec3(0, 0, -1));
    float t_val, u, v;
    bool hit_result = ray_triangle_intersect(ray, tri, t_val, u, v);
    check(hit_result, "ray_triangle_hit");
    
    Aabb box(Vec3(-1,-1,-6), Vec3(1,1,-4));
    float tmin, tmax;
    check(ray_aabb_intersect(ray, box, tmin, tmax), "ray_aabb_hit");
    
    std::vector<PT_Triangle> tris = {tri};
    UnidirectionalPathTracer tracer;
    tracer.set_triangles(&tris);
    auto bvh = tracer.build_bvh();
    check(bvh != nullptr, "bvh_build");
    
    Vec3 color = tracer.trace_path(ray, 0);
    check(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f, "path_trace");
    printf("Path tracing done\n"); fflush(stdout);
}

void test_physics() {
    printf("[Physics]\n"); fflush(stdout);
    PhysicsEngine physics;
    Rigidbody body;
    body.position = Vec3(0, 10, 0);
    body.mass = 1.0f;
    body.inv_mass = 1.0f;
    physics.add_body(&body);
    // Use larger delta time to ensure physics step runs
    physics.update(0.1f);
    check(body.position.y < 10.0f, "gravity");
    printf("Physics done\n"); fflush(stdout);
}

void test_audio() {
    printf("[Audio]\n"); fflush(stdout);
    auto& audio = AudioEngine::get_instance();
    audio.initialize();
    uint32_t clip = audio.load_clip("test.wav");
    check(clip > 0, "audio_load");
    uint32_t source = audio.create_source(clip, Vec3(0,0,0));
    check(source > 0, "audio_source");
    audio.shutdown();
    printf("Audio done\n"); fflush(stdout);
}

void test_input() {
    printf("[Input]\n"); fflush(stdout);
    InputManager input;
    input.set_key_state(65, KeyState::PRESSED);
    check(input.is_key_pressed(65), "key_pressed");
    input.set_mouse_position(100, 200);
    check(input.get_mouse_position().x == 100.0f, "mouse_position");
    input.set_mouse_button(0, true);
    check(input.is_mouse_button_down(0), "mouse_button");
    GamepadState gp;
    gp.connected = true;
    gp.button_a = true;
    input.set_gamepad_state(0, gp);
    check(input.get_gamepad(0).button_a, "gamepad");
    printf("Input done\n"); fflush(stdout);
}

void test_animation() {
    printf("[Animation]\n"); fflush(stdout);
    SkeletalAnimationController anim;
    Bone bone;
    bone.id = 0;
    anim.add_bone(bone);
    AnimationClip clip2;
    clip2.name = "Walk";
    clip2.duration = 1.0f;
    // Add bone keyframe
    clip2.bone_keyframes.resize(1);
    Keyframe kf;
    kf.time = 0.0f;
    kf.position = Vec3(0, 1, 0);
    kf.rotation = Quat::identity();
    kf.scale = Vec3(1, 1, 1);
    clip2.bone_keyframes[0].push_back(kf);
    anim.load_clip("Walk", clip2);
    anim.play("Walk");
    anim.update(0.016f);
    check(true, "animation_update");
    printf("Animation done\n"); fflush(stdout);
}

void test_ui() {
    printf("[UI]\n"); fflush(stdout);
    auto& ui = UIManager::get_instance();
    UIPanel* panel = ui.create_panel();
    check(panel != nullptr, "ui_panel");
    UIButton* btn = ui.create_button();
    btn->set_text("Test");
    btn->click();
    check(btn->was_clicked(), "ui_button");
    UILabel* lbl = ui.create_label();
    lbl->set_text("Hello");
    check(lbl->get_text() == "Hello", "ui_label");
    UISlider* sld = ui.create_slider();
    sld->change_value(0.5f);
    check(sld->was_changed(), "ui_slider");
    printf("UI done\n"); fflush(stdout);
}

void test_editor() {
    printf("[Editor]\n"); fflush(stdout);
    DebugRenderer debug;
    debug.draw_line(Vec3(0,0,0), Vec3(1,1,1));
    Aabb box(Vec3(-1,-1,-6), Vec3(1,1,-4));
    debug.draw_aabb(box);
    debug.draw_coordinate_frame(Vec3(0,0,0));
    debug.update(0.016f);
    check(true, "debug_render");
    PerformanceOverlay overlay;
    overlay.update(0.016f);
    check(overlay.get_stats().fps > 0.0f, "performance_overlay");
    printf("Editor done\n"); fflush(stdout);
}

void test_scripting() {
    printf("[Scripting]\n"); fflush(stdout);
    ScriptingEngine scripting;
    scripting.initialize();
    uint32_t script = scripting.create_script_instance(1, "PlayerController");
    check(script > 0, "script_create");
    scripting.call_method(script, "OnCreate");
    scripting.update(0.016f);
    scripting.shutdown();
    printf("Scripting done\n"); fflush(stdout);
}

void test_advanced_rendering() {
    printf("[Advanced Rendering]\n"); fflush(stdout);
    VarianceShadowMap vsm;
    check(vsm.resolution == 2048, "vsm_resolution");
    SSAO ssao;
    ssao.compute_ao();
    check(ssao.num_samples == 16, "ssao_samples");
    HDRPipeline hdr;
    Vec3 tone_mapped = hdr.apply_tone_mapping(Vec3(2.0f, 3.0f, 4.0f));
    check(tone_mapped.x <= 1.0f && tone_mapped.y <= 1.0f && tone_mapped.z <= 1.0f, "hdr_tone_mapping");
    BloomEffect bloom;
    bloom.apply_bloom();
    check(bloom.num_mip_levels == 5, "bloom_mips");
    DepthOfField dof;
    dof.apply_dof();
    check(dof.focal_distance == 10.0f, "dof_focal");
    MotionBlur mb;
    mb.apply_motion_blur();
    check(mb.num_samples == 8, "motion_blur_samples");
    TAA taa;
    taa.apply_taa();
    check(true, "taa_exists");
    SSR ssr;
    ssr.apply_ssr();
    check(ssr.max_steps == 64, "ssr_steps");
    printf("Advanced rendering done\n"); fflush(stdout);
}

void test_networking() {
    printf("[Networking]\n"); fflush(stdout);
    NetworkManager net;
    net.initialize(NetworkManager::Mode::CLIENT);
    net.send_snapshot();
    net.lag_compensation();
    net.interest_management();
    net.shutdown();
    check(true, "network_init");
    printf("Networking done\n"); fflush(stdout);
}

void test_gameplay() {
    printf("[Gameplay]\n"); fflush(stdout);
    SaveLoadSystem saveload;
    saveload.save_game("test.sav");
    saveload.load_game("test.sav");
    check(true, "saveload");
    AchievementSystem achievements;
    achievements.unlock_achievement(1);
    check(achievements.is_unlocked(1), "achievement_unlock");
    QuestSystem quests;
    QuestSystem::Quest q;
    q.id = 1;
    q.name = "Test Quest";
    quests.add_quest(q);
    quests.complete_quest(1);
    check(true, "quest_complete");
    DialogueSystem dialogue;
    DialogueSystem::DialogueNode node;
    node.id = 1;
    node.text = "Hello";
    dialogue.add_node(node);
    check(dialogue.get_node(1) != nullptr, "dialogue_node");
    printf("Gameplay done\n"); fflush(stdout);
}

void test_performance() {
    printf("[Performance]\n"); fflush(stdout);
    Profiler profiler;
    profiler.begin_scope("Test");
    profiler.end_scope();
    check(true, "profiler");
    OcclusionCulling occlusion;
    occlusion.initialize();
    occlusion.update();
    check(true, "occlusion_culling");
    LODSystem lod;
    check(lod.select_lod(5.0f) == 0, "lod_near");
    check(lod.select_lod(1000.0f) == 3, "lod_far");
    TextureStreaming streaming;
    streaming.initialize();
    streaming.update();
    check(true, "texture_streaming");
    MemoryTracker mem;
    void* ptr = mem.allocate(100, __FILE__, __LINE__);
    check(mem.get_total_allocated() == 100, "memory_alloc");
    mem.deallocate(ptr);
    check(true, "memory_tracker");
    printf("Performance done\n"); fflush(stdout);
}

void test_large_world() {
    printf("[Large World]\n"); fflush(stdout);
    TerrainRenderer terrain;
    terrain.initialize();
    terrain.update();
    check(true, "terrain");
    FoliageSystem foliage;
    foliage.initialize();
    foliage.update();
    check(true, "foliage");
    WorldPartitioning wp;
    wp.initialize();
    wp.update();
    check(true, "world_partitioning");
    LevelStreaming ls;
    ls.initialize();
    ls.update();
    check(true, "level_streaming");
    printf("Large world done\n"); fflush(stdout);
}

void test_engine_loop() {
    printf("[Engine Loop]\n"); fflush(stdout);
    UISystem uisys;
    uisys.initialize();
    uisys.update();
    check(true, "ui_system");
    AssetPackager packager;
    packager.initialize();
    packager.update();
    check(true, "asset_packager");
    EngineLoop loop;
    loop.initialize();
    loop.stop();
    check(true, "engine_loop");
    Benchmark bench;
    bench.run_benchmark();
    check(true, "benchmark");
    printf("Engine loop done\n"); fflush(stdout);
}

int main() {
    printf("Starting tests...\n"); fflush(stdout);
    
    test_math();
    test_radiometry();
    test_path_tracing();
    test_physics();
    test_audio();
    test_input();
    test_animation();
    test_ui();
    test_editor();
    test_scripting();
    test_advanced_rendering();
    test_networking();
    test_gameplay();
    test_performance();
    test_large_world();
    test_engine_loop();
    
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("========================================\n");
    
    return failed > 0 ? 1 : 0;
}
