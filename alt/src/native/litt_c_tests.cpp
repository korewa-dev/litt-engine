// Litt C bridge contract tests
#include "litt_c.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
    if (cond) { ++passed; std::printf("  ok %s\n", name); }
    else { ++failed; std::printf("  FAIL %s\n", name); }
}

static bool near(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

int main() {
    std::printf("[C bridge contract]\n");

    check(litt_abi_version() == LITT_ABI_VERSION, "abi_version_matches_header");
    check((litt_abi_version() >> 16) == LITT_ABI_VERSION_MAJOR, "abi_major_version");

    LittScriptVM* vm = litt_script_vm_create();
    check(vm != nullptr, "script_vm_create");
    check(litt_script_vm_compile(vm, "basic", "var x = 10\nprint x\n") == LITT_OK,
          "script_vm_compile");
    check(litt_script_vm_execute(vm, "basic") == LITT_OK, "script_vm_execute");
    check(litt_script_vm_execute(vm, "missing") == LITT_ERROR_NOT_FOUND,
          "script_vm_missing_is_explicit");
    check(std::strlen(litt_script_vm_last_error(vm)) > 0, "script_vm_error_message");

    check(litt_script_vm_set_limits(vm, 8, 100, 64) == LITT_OK, "script_vm_set_source_limit");
    check(litt_script_vm_compile(vm, "large", "var value = 12345") == LITT_ERROR_LIMIT,
          "script_vm_source_limit_enforced");

    check(litt_script_vm_set_limits(vm, 1024, 100, 64) == LITT_OK,
          "script_vm_reset_limits_for_syntax");
    check(litt_script_vm_compile(vm, "flow", "if false\n") == LITT_ERROR_COMPILE,
          "script_vm_unsupported_flow_is_compile_error");

    check(litt_script_vm_set_limits(vm, 1024, 4, 64) == LITT_OK, "script_vm_set_instruction_limit");
    check(litt_script_vm_compile(vm, "budget", "1\n2\n3\n4\n") == LITT_OK,
          "script_vm_compile_budget_fixture");
    check(litt_script_vm_execute(vm, "budget") == LITT_ERROR_LIMIT,
          "script_vm_instruction_limit_enforced");
    check(litt_script_vm_set_limits(vm, 0, 1, 1) == LITT_ERROR_INVALID_ARGUMENT,
          "script_vm_rejects_zero_limits");
    litt_script_vm_destroy(vm);

    LittWorld* world = litt_world_create(nullptr, nullptr);
    check(world != nullptr, "world_create_empty");

    LittEntityDesc desc{};
    std::snprintf(desc.name, sizeof(desc.name), "%s", "Box");
    desc.position = {1.0f, 2.0f, 3.0f};
    desc.rotation = {0.0f, 0.5f, 0.0f};
    desc.scale = {1.0f, 2.0f, 1.0f};
    desc.color = {1.0f, 0.5f, 0.25f, 1.0f};

    const litt_entity_t id = litt_world_create_entity(world, &desc);
    check(id != UINT32_MAX, "entity_create");

    LittEntityDesc out{};
    check(litt_world_get_entity(world, id, &out), "entity_get");
    check(std::strcmp(out.name, "Box") == 0 &&
          near(out.position.x, 1.0f) && near(out.position.y, 2.0f) &&
          near(out.position.z, 3.0f), "entity_roundtrip");

    litt_entity_t ids[4]{};
    check(litt_world_list_entities(world, ids, 4) == 1 && ids[0] == id,
          "entity_list");

    check(litt_world_has_component(world, id, LITT_COMPONENT_TRANSFORM),
          "transform_component_present");

    check(litt_world_add_component(world, id, LITT_COMPONENT_MESH, nullptr),
          "component_add_null_config");
    check(litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_has_after_null_config");
    check(litt_world_remove_component(world, id, LITT_COMPONENT_MESH),
          "component_remove_after_null_config");

    check(litt_world_add_component(world, id, LITT_COMPONENT_MESH, ""),
          "component_add_empty_config");
    check(litt_world_remove_component(world, id, LITT_COMPONENT_MESH),
          "component_remove_after_empty_config");

    check(litt_world_add_component(world, id, LITT_COMPONENT_MESH, "{}"),
          "component_add_empty_object");
    check(litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_has_after_empty_object");
    check(litt_world_remove_component(world, id, LITT_COMPONENT_MESH),
          "component_remove_after_empty_object");

    check(litt_world_add_component(world, id, LITT_COMPONENT_MESH, " { \n\t } "),
          "component_add_whitespace_empty_object");
    check(litt_world_remove_component(world, id, LITT_COMPONENT_MESH),
          "component_remove_after_whitespace_empty_object");

    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "{\"asset\":\"cube\"}") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_meaningful_object_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "{") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_malformed_json_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "[]") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_array_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "\"mesh\"") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_string_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "42") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_number_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "true") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_boolean_without_mutation");
    check(!litt_world_add_component(world, id, LITT_COMPONENT_MESH, "null") &&
          !litt_world_has_component(world, id, LITT_COMPONENT_MESH),
          "component_rejects_json_null_without_mutation");
    check(!litt_world_add_component(world, id + 1000, LITT_COMPONENT_MESH, "{}"),
          "component_rejects_missing_entity");
    check(!litt_world_add_component(world, id, static_cast<LittComponentType>(999), "{}"),
          "component_rejects_invalid_type");
    check(!litt_world_remove_component(world, id, LITT_COMPONENT_TRANSFORM),
          "transform_component_cannot_remove");

    litt_vec3_t pos{4.0f, 5.0f, 6.0f};
    check(litt_world_set_position(world, id, &pos), "position_set");
    litt_vec3_t got{};
    check(litt_world_get_position(world, id, &got) &&
          near(got.x, 4.0f) && near(got.y, 5.0f) && near(got.z, 6.0f),
          "position_get");

    litt_vec3_t rot{0.25f, 0.5f, 0.75f};
    check(litt_world_set_rotation(world, id, &rot), "rotation_set");
    check(litt_world_get_rotation(world, id, &got) &&
          near(got.x, 0.25f) && near(got.y, 0.5f) && near(got.z, 0.75f),
          "rotation_get");

    litt_vec3_t scale{2.0f, 3.0f, 4.0f};
    check(litt_world_set_scale(world, id, &scale), "scale_set");
    check(litt_world_get_scale(world, id, &got) &&
          near(got.x, 2.0f) && near(got.y, 3.0f) && near(got.z, 4.0f),
          "scale_get");

    litt_vec3_t bad{std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
    check(!litt_world_set_position(world, id, &bad), "position_rejects_nan");
    check(!litt_world_get_position(world, id + 1000, &got), "missing_entity_rejected");

    check(litt_world_start(world), "world_start");
    check(litt_world_is_running(world), "world_running");
    litt_world_step(world, 1.0f / 60.0f);
    check(litt_world_stop(world), "world_stop");
    check(!litt_world_is_running(world), "world_stopped");

    check(litt_world_delete_entity(world, id), "entity_delete");
    check(!litt_world_delete_entity(world, id), "entity_double_delete_rejected");
    litt_world_destroy(world);

    LittEngine* engine = litt_engine_create();
    check(engine != nullptr, "engine_create");
    check(!litt_connect(engine, "127.0.0.1", 8080),
          "unsupported_remote_connect_fails");
    check(!litt_is_connected(engine),
          "failed_remote_connect_not_connected");
    check(litt_get_state(engine) == LITT_ENGINE_STATE_ERROR,
          "failed_remote_connect_reports_error");
    litt_disconnect(engine);
    check(litt_get_state(engine) == LITT_ENGINE_STATE_DISCONNECTED,
          "disconnect_resets_state");
    int width = -1, height = -1;
    unsigned char pixel[4]{};
    check(!litt_engine_get_framebuffer(engine, pixel, &width, &height),
          "framebuffer_unavailable_is_honest");
    check(width == 1920 && height == 1080, "framebuffer_dimensions_reported");
    LittGpuInfo info{};
    litt_engine_get_gpu_info(engine, &info);
    check(std::strcmp(info.name, "Unavailable") == 0 && info.memory_total == 0,
          "gpu_info_unavailable_is_honest");
    litt_engine_destroy(engine);

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
