// Litt C API Implementation
// Bridges C++ Litt Engine to C API for GUI

#include "litt_c.h"
#include "littcore/litt.h"
#include "littcore/litt_engine.h"
#include "littcore/litt_scripting_vm.h"
#include "littcore/litt_json.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <new>

// =============================================================================
// Engine Implementation
// =============================================================================

struct LittEngine {
    litt::Engine engine;
    litt::EngineConfig config;
    LittEngineState state;
    LittQualityPreset quality;
    LittGpuInfo gpu_info;
    LittRenderStats render_stats;
    LittCamera camera;
    int frame_width;
    int frame_height;
    bool frame_valid;
    LittLogCb log_cb;
    void* log_user;
};

struct LittScriptVM {
    litt::ScriptVM vm;
    std::string error;
};

struct LittWorld {
    struct EntityRecord {
        LittEntityDesc desc{};
        uint32_t components = 0;
    };

    litt::WorldManager world;
    bool running = false;
    char scene_path[1024]{};
    char assets_base[1024]{};
    litt_entity_t next_entity_id = 1;
    std::unordered_map<litt_entity_t, EntityRecord> entities;
};

// =============================================================================
// Engine Creation/Destroy
// =============================================================================

LittEngine* litt_engine_create(void) {
    LittEngine* eng = new LittEngine();
    eng->state = LITT_ENGINE_STATE_DISCONNECTED;
    eng->quality = LITT_QUALITY_MEDIUM;
    eng->frame_width = 1920;
    eng->frame_height = 1080;
    eng->frame_valid = false;
    eng->log_cb = nullptr;
    eng->log_user = nullptr;
    
    // Initialize camera defaults
    eng->camera.pos_x = 0.0f;
    eng->camera.pos_y = 5.0f;
    eng->camera.pos_z = 10.0f;
    eng->camera.yaw = 0.0f;
    eng->camera.pitch = -0.3f;
    eng->camera.fov = 60.0f;
    eng->camera.exposure = 1.0f;
    eng->camera.aspect_ratio = 16.0f / 9.0f;
    
    litt_engine_log(eng, "Engine created");
    return eng;
}

void litt_engine_destroy(LittEngine* eng) {
    if (eng) {
        litt_engine_log(eng, "Engine destroyed");
        delete eng;
    }
}

// =============================================================================
// Engine State
// =============================================================================

LittEngineState litt_get_state(LittEngine* eng) {
    return eng ? eng->state : LITT_ENGINE_STATE_DISCONNECTED;
}

const char* litt_state_name(LittEngineState state) {
    switch (state) {
        case LITT_ENGINE_STATE_DISCONNECTED: return "Disconnected";
        case LITT_ENGINE_STATE_CONNECTING: return "Connecting";
        case LITT_ENGINE_STATE_CONNECTED: return "Connected";
        case LITT_ENGINE_STATE_RUNNING: return "Running";
        case LITT_ENGINE_STATE_PAUSED: return "Paused";
        case LITT_ENGINE_STATE_ERROR: return "Error";
        default: return "Unknown";
    }
}

// =============================================================================
// Connection
// =============================================================================

bool litt_connect(LittEngine* eng, const char* host, int port) {
    if (!eng) return false;

    // The release-supported C bridge is an in-process API. It does not own a
    // transport implementation yet, so claiming a successful TCP connection
    // would violate Litt's honest-failure contract.
    eng->state = LITT_ENGINE_STATE_ERROR;
    litt_engine_log(eng,
        "Remote connection unavailable (host=%s port=%d): no release-supported transport backend",
        host ? host : "(null)", port);
    return false;
}

void litt_disconnect(LittEngine* eng) {
    if (!eng) return;
    
    eng->state = LITT_ENGINE_STATE_DISCONNECTED;
    litt_engine_log(eng, "Disconnected");
}

bool litt_is_connected(LittEngine* eng) {
    return eng && eng->state == LITT_ENGINE_STATE_CONNECTED;
}

// =============================================================================
// World Management
// =============================================================================

LittWorld* litt_world_create(const char* scene_path, const char* assets_base) {
    LittWorld* world = new (std::nothrow) LittWorld();
    if (!world) return nullptr;

    if (assets_base) {
        std::snprintf(world->assets_base, sizeof(world->assets_base), "%s", assets_base);
    }

    if (scene_path && scene_path[0] != '\0') {
        if (!litt_world_load(world, scene_path)) {
            delete world;
            return nullptr;
        }
    }

    return world;
}

void litt_world_destroy(LittWorld* world) {
    delete world;
}

bool litt_world_load(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path || scene_path[0] == '\0') return false;
    if (!world->world.load(scene_path)) return false;
    std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path);
    return true;
}

bool litt_world_save(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path || scene_path[0] == '\0') return false;
    if (!world->world.save(scene_path)) return false;
    std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path);
    return true;
}

// =============================================================================
// Entity Management
// =============================================================================

static uint32_t component_bit(LittComponentType type) {
    const uint32_t value = static_cast<uint32_t>(type);
    return (value >= 1 && value <= 31) ? (1u << value) : 0u;
}

static LittWorld::EntityRecord* find_entity(LittWorld* world, litt_entity_t id) {
    if (!world) return nullptr;
    const auto it = world->entities.find(id);
    return it == world->entities.end() ? nullptr : &it->second;
}

static const LittWorld::EntityRecord* find_entity(const LittWorld* world, litt_entity_t id) {
    if (!world) return nullptr;
    const auto it = world->entities.find(id);
    return it == world->entities.end() ? nullptr : &it->second;
}

litt_entity_t litt_world_create_entity(LittWorld* world, const LittEntityDesc* desc) {
    if (!world || !desc) return UINT32_MAX;
    if (!std::isfinite(desc->position.x) || !std::isfinite(desc->position.y) ||
        !std::isfinite(desc->position.z) || !std::isfinite(desc->rotation.x) ||
        !std::isfinite(desc->rotation.y) || !std::isfinite(desc->rotation.z) ||
        !std::isfinite(desc->scale.x) || !std::isfinite(desc->scale.y) ||
        !std::isfinite(desc->scale.z)) {
        return UINT32_MAX;
    }

    litt_entity_t id = world->next_entity_id++;
    if (id == UINT32_MAX) id = world->next_entity_id++;

    LittWorld::EntityRecord record;
    record.desc = *desc;
    record.desc.name[sizeof(record.desc.name) - 1] = '\0';
    record.components = component_bit(LITT_COMPONENT_TRANSFORM);
    world->entities.emplace(id, record);
    return id;
}

bool litt_world_delete_entity(LittWorld* world, litt_entity_t entity_id) {
    return world && world->entities.erase(entity_id) == 1;
}

bool litt_world_get_entity(LittWorld* world, litt_entity_t entity_id, LittEntityDesc* out) {
    if (!out) return false;
    const auto* record = find_entity(world, entity_id);
    if (!record) return false;
    *out = record->desc;
    return true;
}

int litt_world_list_entities(LittWorld* world, litt_entity_t* ids, int max_count) {
    if (!world || max_count < 0 || (max_count > 0 && !ids)) return 0;
    std::vector<litt_entity_t> sorted;
    sorted.reserve(world->entities.size());
    for (const auto& pair : world->entities) sorted.push_back(pair.first);
    std::sort(sorted.begin(), sorted.end());

    const int count = std::min<int>(max_count, static_cast<int>(sorted.size()));
    for (int i = 0; i < count; ++i) ids[i] = sorted[static_cast<size_t>(i)];
    return count;
}

bool litt_world_add_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type, const char* config_json) {
    auto* record = find_entity(world, entity_id);
    const uint32_t bit = component_bit(type);
    if (!record || bit == 0) return false;

    if (config_json && config_json[0] != '\0') {
        LvJson* config = lvj_parse_strict(config_json);
        if (!config) return false;
        const bool is_default_config = config->kind == LJ_OBJ && config->count == 0;
        lvj_free(config);
        if (!is_default_config) return false;
    }

    record->components |= bit;
    return true;
}

bool litt_world_remove_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type) {
    auto* record = find_entity(world, entity_id);
    const uint32_t bit = component_bit(type);
    if (!record || bit == 0 || type == LITT_COMPONENT_TRANSFORM) return false;
    const bool had = (record->components & bit) != 0;
    record->components &= ~bit;
    return had;
}

bool litt_world_has_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type) {
    const auto* record = find_entity(world, entity_id);
    const uint32_t bit = component_bit(type);
    return record && bit != 0 && (record->components & bit) != 0;
}

bool litt_world_set_position(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* pos) {
    auto* record = find_entity(world, entity_id);
    if (!record || !pos || !std::isfinite(pos->x) || !std::isfinite(pos->y) || !std::isfinite(pos->z)) return false;
    record->desc.position = *pos;
    return true;
}

bool litt_world_get_position(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    const auto* record = find_entity(world, entity_id);
    if (!record || !out) return false;
    *out = record->desc.position;
    return true;
}

bool litt_world_set_rotation(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* rot) {
    auto* record = find_entity(world, entity_id);
    if (!record || !rot || !std::isfinite(rot->x) || !std::isfinite(rot->y) || !std::isfinite(rot->z)) return false;
    record->desc.rotation = *rot;
    return true;
}

bool litt_world_get_rotation(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    const auto* record = find_entity(world, entity_id);
    if (!record || !out) return false;
    *out = record->desc.rotation;
    return true;
}

bool litt_world_set_scale(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* scale) {
    auto* record = find_entity(world, entity_id);
    if (!record || !scale || !std::isfinite(scale->x) || !std::isfinite(scale->y) || !std::isfinite(scale->z)) return false;
    record->desc.scale = *scale;
    return true;
}

bool litt_world_get_scale(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    const auto* record = find_entity(world, entity_id);
    if (!record || !out) return false;
    *out = record->desc.scale;
    return true;
}

// =============================================================================
// Simulation
// =============================================================================

bool litt_world_start(LittWorld* world) {
    if (!world) return false;
    world->running = true;
    return true;
}

bool litt_world_stop(LittWorld* world) {
    if (!world) return false;
    world->running = false;
    return true;
}

bool litt_world_is_running(LittWorld* world) {
    return world && world->running;
}

void litt_world_step(LittWorld* world, float dt) {
    if (!world || !world->running || !std::isfinite(dt) || dt <= 0.0f) return;
    world->world.update(dt);
}

// =============================================================================
// Rendering
// =============================================================================

void litt_engine_set_quality(LittEngine* eng, LittQualityPreset quality) {
    if (!eng) return;
    eng->quality = quality;
}

LittQualityPreset litt_engine_get_quality(LittEngine* eng) {
    return eng ? eng->quality : LITT_QUALITY_MEDIUM;
}

void litt_engine_get_gpu_info(LittEngine* eng, LittGpuInfo* info) {
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    if (!eng) return;
    std::snprintf(info->name, sizeof(info->name), "%s", "Unavailable");
    std::snprintf(info->vendor, sizeof(info->vendor), "%s", "Unavailable");
}

void litt_engine_get_render_stats(LittEngine* eng, LittRenderStats* stats) {
    if (!stats) return;
    std::memset(stats, 0, sizeof(*stats));
    if (!eng) return;
    stats->width = static_cast<uint32_t>(std::max(0, eng->frame_width));
    stats->height = static_cast<uint32_t>(std::max(0, eng->frame_height));
}

void litt_engine_get_camera(LittEngine* eng, LittCamera* cam) {
    if (!eng || !cam) return;
    *cam = eng->camera;
}

void litt_engine_set_camera(LittEngine* eng, const LittCamera* cam) {
    if (!eng || !cam) return;
    if (!std::isfinite(cam->pos_x) || !std::isfinite(cam->pos_y) || !std::isfinite(cam->pos_z) ||
        !std::isfinite(cam->yaw) || !std::isfinite(cam->pitch) || !std::isfinite(cam->fov) ||
        !std::isfinite(cam->exposure) || !std::isfinite(cam->aspect_ratio) ||
        cam->fov <= 0.0f || cam->fov >= 180.0f || cam->aspect_ratio <= 0.0f) {
        return;
    }
    eng->camera = *cam;
}

bool litt_engine_get_framebuffer(LittEngine* eng, uint8_t* buf, int* width, int* height) {
    if (width) *width = eng ? eng->frame_width : 0;
    if (height) *height = eng ? eng->frame_height : 0;
    if (!eng || !buf || !eng->frame_valid) return false;
    return false;
}

// =============================================================================
// Logging
// =============================================================================

void litt_engine_set_log_callback(LittEngine* eng, LittLogCb cb, void* user) {
    if (!eng) return;
    eng->log_cb = cb;
    eng->log_user = user;
}

void litt_engine_log(LittEngine* eng, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    fprintf(stderr, "[litt] %s\n", buf);
    
    if (eng && eng->log_cb) {
        eng->log_cb(buf, eng->log_user);
    }
}

// =============================================================================
// Scripting VM
// =============================================================================

LittScriptVM* litt_script_vm_create(void) {
    LittScriptVM* handle = new (std::nothrow) LittScriptVM();
    if (!handle) return nullptr;
    if (!handle->vm.initialize()) {
        delete handle;
        return nullptr;
    }
    return handle;
}

void litt_script_vm_destroy(LittScriptVM* vm) {
    if (!vm) return;
    vm->vm.shutdown();
    delete vm;
}

LittResult litt_script_vm_set_limits(LittScriptVM* vm, size_t max_source_bytes,
                                     size_t max_instructions, size_t max_stack_values) {
    if (!vm || max_source_bytes == 0 || max_instructions == 0 || max_stack_values == 0)
        return LITT_ERROR_INVALID_ARGUMENT;
    vm->error.clear();
    vm->vm.set_limits(max_source_bytes, max_instructions, max_stack_values);
    return LITT_OK;
}

LittResult litt_script_vm_compile(LittScriptVM* vm, const char* script_name,
                                  const char* source) {
    if (!vm || !script_name || !source || script_name[0] == '\0')
        return LITT_ERROR_INVALID_ARGUMENT;
    vm->error.clear();
    if (vm->vm.compile(script_name, source)) return LITT_OK;
    vm->error = vm->vm.last_error();
    return vm->error.find("limit") != std::string::npos ||
           vm->error.find("exceeds") != std::string::npos
        ? LITT_ERROR_LIMIT : LITT_ERROR_COMPILE;
}

LittResult litt_script_vm_execute(LittScriptVM* vm, const char* script_name) {
    if (!vm || !script_name || script_name[0] == '\0')
        return LITT_ERROR_INVALID_ARGUMENT;
    vm->error.clear();
    if (vm->vm.execute(script_name)) return LITT_OK;
    vm->error = vm->vm.last_error();
    if (vm->error == "script not found") return LITT_ERROR_NOT_FOUND;
    return vm->error.find("limit") != std::string::npos
        ? LITT_ERROR_LIMIT : LITT_ERROR_RUNTIME;
}

const char* litt_script_vm_last_error(const LittScriptVM* vm) {
    return vm ? vm->error.c_str() : "invalid VM handle";
}

// =============================================================================
// Version / ABI
// =============================================================================

const char* litt_version(void) {
    return "1.1.0";
}

uint32_t litt_abi_version(void) {
    return LITT_ABI_VERSION;
}
