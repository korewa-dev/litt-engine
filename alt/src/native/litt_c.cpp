// Litt C API Implementation
// Bridges C++ Litt Engine to C API for GUI

#include "litt_c.h"
#include "littcore/litt.h"
#include "littcore/litt_engine.h"
#include "littcore/litt_scripting_vm.h"
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

LittEngineState litt_get_state(LittEngine* eng) { return eng ? eng->state : LITT_ENGINE_STATE_DISCONNECTED; }

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

bool litt_connect(LittEngine* eng, const char* host, int port) {
    if (!eng) return false;
    eng->state = LITT_ENGINE_STATE_ERROR;
    litt_engine_log(eng, "Remote connection unavailable (host=%s port=%d): no release-supported transport backend", host ? host : "(null)", port);
    return false;
}

void litt_disconnect(LittEngine* eng) { if (eng) { eng->state = LITT_ENGINE_STATE_DISCONNECTED; litt_engine_log(eng, "Disconnected"); } }
bool litt_is_connected(LittEngine* eng) { return eng && eng->state == LITT_ENGINE_STATE_CONNECTED; }

LittWorld* litt_world_create(const char* scene_path, const char* assets_base) {
    LittWorld* world = new (std::nothrow) LittWorld();
    if (!world) return nullptr;
    if (assets_base) std::snprintf(world->assets_base, sizeof(world->assets_base), "%s", assets_base);
    if (scene_path && scene_path[0] != '\0' && !litt_world_load(world, scene_path)) { delete world; return nullptr; }
    return world;
}

void litt_world_destroy(LittWorld* world) { delete world; }
bool litt_world_load(LittWorld* world, const char* scene_path) { if (!world || !scene_path || scene_path[0] == '\0' || !world->world.load(scene_path)) return false; std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path); return true; }
bool litt_world_save(LittWorld* world, const char* scene_path) { if (!world || !scene_path || scene_path[0] == '\0' || !world->world.save(scene_path)) return false; std::snprintf(world->scene_path, sizeof(world->scene_path), "%s", scene_path); return true; }

static uint32_t component_bit(LittComponentType type) {
    const uint32_t value = static_cast<uint32_t>(type);
    return (value >= 1 && value <= 31) ? (1u << value) : 0u;
}

static bool c_bridge_component_supported(LittComponentType type) {
    return type == LITT_COMPONENT_TRANSFORM;
}

static LittWorld::EntityRecord* find_entity(LittWorld* world, litt_entity_t id) { if (!world) return nullptr; const auto it = world->entities.find(id); return it == world->entities.end() ? nullptr : &it->second; }
static const LittWorld::EntityRecord* find_entity(const LittWorld* world, litt_entity_t id) { if (!world) return nullptr; const auto it = world->entities.find(id); return it == world->entities.end() ? nullptr : &it->second; }

litt_entity_t litt_world_create_entity(LittWorld* world, const LittEntityDesc* desc) {
    if (!world || !desc) return UINT32_MAX;
    if (!std::isfinite(desc->position.x) || !std::isfinite(desc->position.y) || !std::isfinite(desc->position.z) || !std::isfinite(desc->rotation.x) || !std::isfinite(desc->rotation.y) || !std::isfinite(desc->rotation.z) || !std::isfinite(desc->scale.x) || !std::isfinite(desc->scale.y) || !std::isfinite(desc->scale.z)) return UINT32_MAX;
    litt_entity_t id = world->next_entity_id++;
    if (id == UINT32_MAX) id = world->next_entity_id++;
    LittWorld::EntityRecord record;
    record.desc = *desc;
    record.desc.name[sizeof(record.desc.name) - 1] = '\0';
    record.components = component_bit(LITT_COMPONENT_TRANSFORM);
    world->entities.emplace(id, record);
    return id;
}

bool litt_world_delete_entity(LittWorld* world, litt_entity_t entity_id) { return world && world->entities.erase(entity_id) == 1; }
bool litt_world_get_entity(LittWorld* world, litt_entity_t entity_id, LittEntityDesc* out) { if (!out) return false; const auto* record = find_entity(world, entity_id); if (!record) return false; *out = record->desc; return true; }

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
    if (!record || !c_bridge_component_supported(type)) return false;
    if (config_json && config_json[0] != '\0' && std::strcmp(config_json, "{}") != 0) return false;
    record->components |= component_bit(type);
    return true;
}

bool litt_world_remove_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type) {
    auto* record = find_entity(world, entity_id);
    if (!record || !c_bridge_component_supported(type) || type == LITT_COMPONENT_TRANSFORM) return false;
    const uint32_t bit = component_bit(type);
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

bool litt_world_get_position(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) { if (!out) return false; const auto* record = find_entity(world, entity_id); if (!record) return false; *out = record->desc.position; return true; }

bool litt_world_start(LittWorld* world) { if (!world) return false; world->running = true; return true; }
bool litt_world_stop(LittWorld* world) { if (!world) return false; world->running = false; return true; }
bool litt_world_is_running(LittWorld* world) { return world && world->running; }
void litt_world_step(LittWorld* world, float dt) { if (world && world->running && std::isfinite(dt) && dt >= 0.0f) world->world.update(dt); }

// The remainder of this translation unit is intentionally unchanged by this audit repair.
