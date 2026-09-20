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
#include "litt_scripting_vm.h"
#include "litt_event.h"
#include "litt_serialization.h"
#include "litt_scripting.h"
#include "litt_networking.h"
#include "litt_audio.h"
#include <fstream>
#ifdef _WIN32
#include "litt_gpu_software.h"
#endif

using namespace litt;

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

static bool near_float(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static bool near_vec(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return near_float(a.x, b.x, eps) &&
           near_float(a.y, b.y, eps) &&
           near_float(a.z, b.z, eps);
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

static void test_physics_registration() {
    PhysicsBody a = body_at(Vec3(0, 0, 0));
    PhysicsBody b = body_at(Vec3(0.5f, 0, 0));
    PhysicsSystem physics(0.0f, Vec3::zero());

    check(physics.addBody(&a), "physics_first_registration_succeeds");
    check(!physics.addBody(&a), "physics_duplicate_registration_rejected");
    check(physics.bodies.size() == 1 && physics.broadPhase.bodies.size() == 1,
          "physics_duplicate_not_stored_twice");

    check(physics.addBody(&b), "physics_second_body_registration_succeeds");
    physics.update();
    check(physics.narrowPhase.contacts.size() == 1,
          "physics_duplicate_does_not_create_repeated_contacts");

    physics.removeBody(&a);
    check(physics.bodies.size() == 1 && physics.broadPhase.bodies.size() == 1,
          "physics_remove_after_duplicate_attempt_unregisters_once");
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
        check(near_float(a.aabb.center().x, a.centerOfMass.x) &&
              near_float(b.aabb.center().x, b.centerOfMass.x),
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
    check(near_float(roundtrip.x, p.x) && near_float(roundtrip.y, p.y) && near_float(roundtrip.z, p.z),
          "affine_inverse_nonuniform_trs");
}


#ifdef _WIN32
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
#endif





static void write_test_wav(const char* path) {
    const unsigned char wav[] = {
        'R','I','F','F', 38,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0,
        1,0, 1,0, 0x40,0x1F,0,0,
        0x40,0x1F,0,0, 1,0, 8,0,
        'd','a','t','a', 1,0,0,0,
        128, 0
    };
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(wav), sizeof(wav));
}

static void test_audio_facade() {
    const char* path = "litt_test_audio.wav";
    write_test_wav(path);

    AudioManager audio;
    audio.init();
    auto clip = audio.loadClip(path);
    check(clip != nullptr, "audio_wav_load");
    check(clip && clip->sampleRate == 8000 && clip->channels == 1 &&
          clip->lengthSamples == 1 && clip->data.size() == 1,
          "audio_wav_metadata");
    check(clip && near_float(clip->duration(), 1.0f / 8000.0f, 1e-7f),
          "audio_wav_duration");

    audio.setMasterVolume(2.0f);
    check(near_float(audio.getMasterVolume(), 1.0f), "audio_master_volume_clamp_high");
    audio.setMasterVolume(-1.0f);
    check(near_float(audio.getMasterVolume(), 0.0f), "audio_master_volume_clamp_low");

    bool missing_rejected = audio.loadClip("does-not-exist.wav") == nullptr;
    check(missing_rejected, "audio_missing_file_rejected");

    bool source_failed = false;
    try { (void)audio.addSource("missing", "does-not-exist.wav"); }
    catch (const std::runtime_error&) { source_failed = true; }
    check(source_failed, "audio_source_missing_clip_rejected");

    if (clip) {
        AudioSource source;
        source.clip = clip;
        source.play();
        source.update(1.0f);
        check(source.state == AudioState::Stopped, "audio_source_stops_at_end");
    }

    std::remove(path);
}

static void test_networking_facade() {
    auto& server = NetworkServer::get_instance();
    check(!server.start(7777), "network_server_reports_unavailable");
    NetworkMessage message{};
    check(!server.broadcast(message), "network_broadcast_reports_unavailable");
    server.stop();

    auto& client = NetworkClient::get_instance();
    check(!client.connect("127.0.0.1", 7777), "network_client_reports_unavailable");
    check(!client.is_connected(), "network_client_not_fake_connected");
    check(!client.send(message), "network_send_reports_unavailable");

    auto& manager = NetworkManager::get_instance();
    check(!manager.initialize(), "network_manager_reports_unavailable");
    check(manager.create_server() == nullptr && manager.create_client() == nullptr,
          "network_manager_does_not_create_fake_transport");
    manager.shutdown();
}

static void test_serialization_api() {
    JSONSerializer json;
    json.set_data("{\"ok\":true}");
    const auto json_bytes = json.serialize_to_buffer();
    JSONSerializer json_copy;
    check(json_copy.deserialize_from_buffer(json_bytes) &&
          json_copy.get_data() == "{\"ok\":true}",
          "serialization_json_buffer_roundtrip");

    BinarySerializer binary;
    binary.write_uint(0x78563412u);
    binary.write_int(-17);
    binary.write_float(3.25f);
    binary.write_bool(true);
    binary.write_string("litt");
    const auto bytes = binary.serialize_to_buffer();

    BinarySerializer read;
    check(read.deserialize_from_buffer(bytes), "serialization_binary_load_buffer");
    check(read.read_uint() == 0x78563412u, "serialization_uint_roundtrip");
    check(read.read_int() == -17, "serialization_int_roundtrip");
    check(near_float(read.read_float(), 3.25f), "serialization_float_roundtrip");
    check(read.read_bool(), "serialization_bool_roundtrip");
    check(read.read_string() == "litt", "serialization_string_roundtrip");
    check(read.read_bytes(999999).empty(), "serialization_bounds_guard");

    auto& manager = SerializationManager::get_instance();
    auto managed = std::make_unique<JSONSerializer>();
    managed->set_data("managed");
    manager.register_serializer("json-test", std::move(managed));
    check(manager.serialize_to_buffer("json-test") ==
          std::vector<uint8_t>({'m','a','n','a','g','e','d'}),
          "serialization_manager_dispatch");
    manager.register_serializer("json-test", nullptr);
    check(manager.get_serializer("json-test") == nullptr,
          "serialization_manager_remove");
}

static void test_scripting_facade() {
    ScriptContext context;
    context.set_variable("speed", 4.5f);
    check(context.has_variable("speed") && near_float(context.get_float("speed"), 4.5f),
          "script_context_float");
    context.set_variable("speed", std::string("fast"));
    check(context.get_string("speed") == "fast" && near_float(context.get_float("speed"), 0.0f),
          "script_context_type_replacement");

    auto& engine = ScriptEngine::get_instance();
    check(engine.initialize(), "script_facade_initialize");
    int callback_calls = 0;
    engine.register_function("tick", [&](ScriptContext& ctx) {
        ++callback_calls;
        ctx.set_variable("called", true);
    });
    check(engine.execute_function("tick", context) &&
          callback_calls == 1 && context.get_bool("called"),
          "script_facade_registered_function");
    check(engine.execute("var x = 10\nprint x\n"),
          "script_facade_vm_execution");
    check(!engine.execute_function("missing", context),
          "script_facade_missing_function_rejected");
    engine.shutdown();
}

static void test_event_dispatcher() {
    EventDispatcher dispatcher;
    int calls = 0;

    const auto id = dispatcher.subscribe<StringEvent>(
        [&](const StringEvent& event) {
            if (event.message == "ping") ++calls;
        });

    check(id != 0, "event_subscription_returns_id");
    check(dispatcher.subscriber_count() == 1, "event_subscription_count");
    dispatcher.dispatch_string("ping");
    check(calls == 1, "event_dispatch_invokes_listener");
    check(dispatcher.unsubscribe<StringEvent>(id), "event_unsubscribe_by_id");
    check(dispatcher.subscriber_count() == 0, "event_unsubscribe_removes_listener");
    dispatcher.dispatch_string("ping");
    check(calls == 1, "event_unsubscribed_listener_not_called");
    check(!dispatcher.unsubscribe<StringEvent>(id), "event_duplicate_unsubscribe_rejected");

    const auto a = dispatcher.subscribe<StringEvent>([](const StringEvent&) {});
    const auto b = dispatcher.subscribe<StringEvent>([](const StringEvent&) {});
    check(a != b && dispatcher.unsubscribe_all<StringEvent>() == 2,
          "event_unsubscribe_all");
}

static void test_scripting_vm() {
    ScriptComponent component;
    component.set_enabled(false);
    check(!component.is_enabled(), "script_component_disable");
    component.set_enabled(true);
    check(component.is_enabled(), "script_component_enable");

    ScriptVM vm;
    check(vm.initialize(), "script_vm_initialize");
    check(vm.compile("basic", "var x = 10\nprint x\n"), "script_vm_compile_basic");
    check(vm.execute("basic"), "script_vm_execute_basic");
    check(vm.stack_size() == 0, "script_vm_stack_cleanup");
    for (int i = 0; i < 32; ++i) check(vm.execute("basic"), "script_vm_repeat_execute");
    check(vm.stack_size() == 0, "script_vm_repeat_no_stack_growth");
    check(!vm.compile("bad-control", "if true\nprint 1\nend\n"),
          "script_vm_rejects_unimplemented_control_flow");
    check(!vm.compile("bad-number", "print 12oops\n"),
          "script_vm_rejects_malformed_number");
    VMFunction malformed("malformed");
    malformed.bytecode.push_back(static_cast<uint8_t>(OpCode::PUSH_FLOAT));
    check(!vm.executeFunction(&malformed), "script_vm_rejects_truncated_bytecode");
    check(vm.stack_size() == 0, "script_vm_malformed_cleanup");
    check(!vm.execute("missing"), "script_vm_missing_rejected");
    vm.shutdown();
}

static void test_quaternion_transform_contract() {
    const Quat q = Quat::from_axis_angle(Vec3::unit_y(), 3.14159265358979323846f * 0.5f);

    Transform ecs;
    ecs.position = Vec3(2, 3, 4);
    ecs.rotation = q;
    ecs.scale = Vec3(2, 1, 0.5f);
    ecs.update();

    Scene scene;
    SceneNode& node = scene.createNode("QuaternionTransform");
    node.position = ecs.position;
    node.rotation = q;
    node.scale = ecs.scale;
    node.updateTransform();

    const Vec3 probe(1, 0, 0);
    check(near_vec(ecs.matrix * probe, node.transform * probe),
          "scene_ecs_quaternion_trs_matches");
}

static void test_scene_component_ownership() {
    Scene scene;
    SceneNode& node = scene.createNode("OwnedComponents");

    MeshData mesh;
    mesh.name = "owned_mesh";
    mesh.positions.push_back(Vec3(1, 2, 3));
    RenderMaterial material;
    material.name = "owned_material";
    RenderCamera camera;
    camera.fov = 73.0f;

    node.setMesh(mesh);
    node.setMaterial(material);
    node.setCamera(camera);

    // Mutating/destroying caller-side values must not alter scene attachments.
    mesh.name = "caller_mesh";
    mesh.positions[0] = Vec3::zero();
    material.name = "caller_material";
    camera.fov = 10.0f;

    check(node.meshComponent && node.meshComponent->name == "owned_mesh",
          "scene_owns_mesh_attachment");
    check(node.meshComponent && near_vec(node.meshComponent->positions[0], Vec3(1, 2, 3)),
          "scene_mesh_attachment_is_independent_copy");
    check(node.materialComponent && node.materialComponent->name == "owned_material",
          "scene_owns_material_attachment");
    check(node.cameraComponent && near_float(node.cameraComponent->fov, 73.0f),
          "scene_owns_camera_attachment");
}

static void test_scene_lifecycle() {
    SceneManager manager;
    Scene& first = manager.createScene("Level");
    manager.setActiveScene("Level");
    Scene& duplicate = manager.createScene("Level");
    check(&first == &duplicate && manager.getActiveScene() == &first,
          "scene_duplicate_does_not_replace_active");

    SceneNode& parent = first.createNode("Parent");
    SceneNode& child = first.createNode("Child");
    const uint32_t parent_id = parent.id;
    const uint32_t child_id = child.id;
    check(first.setParent(child_id, parent_id) &&
          child.parent == &parent &&
          parent.children.size() == 1 &&
          parent.children[0] == &child &&
          first.getNode(child_id) == &child,
          "scene_registry_owns_hierarchy_nodes");
    check(!first.setParent(parent_id, child_id),
          "scene_hierarchy_rejects_cycles");
    first.removeNode(parent_id);
    check(first.getNode(parent_id) == nullptr &&
          first.getNode(child_id) == nullptr,
          "scene_remove_parent_removes_owned_subtree");
    check(first.root != nullptr && first.getNode(first.root->id) == first.root,
          "scene_root_survives_subtree_removal");

    check(first.serializeToJson().empty(), "scene_serialize_reports_unavailable");
    check(!first.deserializeFromJson("{}"), "scene_deserialize_reports_unavailable");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Stabilization]\n");
    test_physics_normals();
    test_physics_registration();
    test_physics_resolution();
    test_memory();
    test_ecs_generations();
    test_affine_inverse();
    test_scripting_vm();
    test_event_dispatcher();
    test_audio_facade();
    test_networking_facade();
    test_serialization_api();
    test_scripting_facade();
#ifdef _WIN32
    test_software_renderer_hardening();
#endif
    test_quaternion_transform_contract();
    test_scene_component_ownership();
    test_scene_lifecycle();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
