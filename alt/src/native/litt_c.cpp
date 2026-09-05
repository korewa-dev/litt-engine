// Litt C API Implementation
// Bridges C++ Litt Engine to C API for GUI

#include "litt_c.h"
#include "littcore/litt.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>

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
    uint8_t frame_buffer[1920 * 1080 * 4];
    int frame_width;
    int frame_height;
    bool frame_valid;
    LittLogCb log_cb;
    void* log_user;
};

struct LittWorld {
    litt::WorldManager world;
    bool running;
    char scene_path[1024];
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
    
    litt_engine_log(eng, "Connecting to %s:%d", host, port);
    eng->state = LITT_ENGINE_STATE_CONNECTING;
    
    // Simulate connection (real implementation would use TCP)
    // For now, just set as connected
    eng->state = LITT_ENGINE_STATE_CONNECTED;
    litt_engine_log(eng, "Connected to %s:%d", host, port);
    
    return true;
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
    LittWorld* world = new LittWorld();
    
    if (scene_path) {
        strncpy(world->scene_path, scene_path, sizeof(world->scene_path) - 1);
        world->scene_path[sizeof(world->scene_path) - 1] = '\0';
    } else {
        world->scene_path[0] = '\0';
    }
    
    world->running = false;
    
    // Load scene if path provided
    if (scene_path && scene_path[0] != '\0') {
        // World loading would happen here
    }
    
    return world;
}

void litt_world_destroy(LittWorld* world) {
    if (world) {
        delete world;
    }
}

bool litt_world_load(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path) return false;
    
    strncpy(world->scene_path, scene_path, sizeof(world->scene_path) - 1);
    world->scene_path[sizeof(world->scene_path) - 1] = '\0';
    
    // Scene loading would parse JSON and create entities
    return true;
}

bool litt_world_save(LittWorld* world, const char* scene_path) {
    if (!world || !scene_path) return false;
    
    // Scene saving would serialize entities to JSON
    return true;
}

// =============================================================================
// Entity Management
// =============================================================================

static litt::EntityId entity_to_id(litt_entity_t id) {
    return static_cast<litt::EntityId>(id);
}

static litt_entity_t id_to_entity(litt::EntityId id) {
    return static_cast<litt_entity_t>(id);
}

litt_entity_t litt_world_create_entity(LittWorld* world, const LittEntityDesc* desc) {
    if (!world || !desc) return 0xFFFFFFFF;
    
    // Create entity with basic transform
    litt::Vec3 pos(desc->position.x, desc->position.y, desc->position.z);
    litt::Vec3 rot(desc->rotation.x, desc->rotation.y, desc->rotation.z);
    litt::Vec3 scale(desc->scale.x, desc->scale.y, desc->scale.z);
    
    // For now, return a fake ID (real implementation would use ECS)
    static litt_entity_t next_id = 1;
    litt_entity_t id = next_id++;
    
    litt_engine_log(nullptr, "Created entity %u: %s", id, desc->name);
    return id;
}

bool litt_world_delete_entity(LittWorld* world, litt_entity_t entity_id) {
    if (!world) return false;
    
    // Entity deletion would remove from ECS
    return true;
}

bool litt_world_get_entity(LittWorld* world, litt_entity_t entity_id, LittEntityDesc* out) {
    if (!world || !out) return false;
    
    // Entity retrieval would query ECS
    // For now, return empty
    return false;
}

int litt_world_list_entities(LittWorld* world, litt_entity_t* ids, int max_count) {
    if (!world || !ids) return 0;
    
    // List entities would query ECS
    // For now, return 0
    return 0;
}

// =============================================================================
// Component Management
// =============================================================================

bool litt_world_add_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type, const char* config_json) {
    if (!world) return false;
    
    // Component addition would add to ECS
    return true;
}

bool litt_world_remove_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type) {
    if (!world) return false;
    
    // Component removal would remove from ECS
    return true;
}

bool litt_world_has_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type) {
    if (!world) return false;
    
    // Component query would check ECS
    return false;
}

// =============================================================================
// Transform Operations
// =============================================================================

bool litt_world_set_position(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* pos) {
    if (!world || !pos) return false;
    // Transform update would go through ECS
    return true;
}

bool litt_world_get_position(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    if (!world || !out) return false;
    return true;
}

bool litt_world_set_rotation(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* rot) {
    if (!world || !rot) return false;
    return true;
}

bool litt_world_get_rotation(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    if (!world || !out) return false;
    return true;
}

bool litt_world_set_scale(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* scale) {
    if (!world || !scale) return false;
    return true;
}

bool litt_world_get_scale(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out) {
    if (!world || !out) return false;
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
    if (!world) return;
    // World step would update physics, etc.
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
    if (!eng || !info) return;
    
    // Real GPU info would query Vulkan
    strncpy(info->name, "Mock GPU", sizeof(info->name) - 1);
    strncpy(info->vendor, "Mock Vendor", sizeof(info->vendor) - 1);
    info->memory_total = 8 * 1024 * 1024 * 1024; // 8GB
    info->memory_free = 6 * 1024 * 1024 * 1024;  // 6GB
    info->max_bounces = 16;
    info->max_texture_size = 8192;
}

void litt_engine_get_render_stats(LittEngine* eng, LittRenderStats* stats) {
    if (!eng || !stats) return;
    
    // Real stats would come from engine
    stats->fps = 60.0f;
    stats->frame_time_ms = 16.67f;
    stats->path_time_ms = 0.0f;
    stats->spp = 1;
    stats->bounces = 4;
    stats->width = eng->frame_width;
    stats->height = eng->frame_height;
}

void litt_engine_get_camera(LittEngine* eng, LittCamera* cam) {
    if (!eng || !cam) return;
    *cam = eng->camera;
}

void litt_engine_set_camera(LittEngine* eng, const LittCamera* cam) {
    if (!eng || !cam) return;
    eng->camera = *cam;
}

bool litt_engine_get_framebuffer(LittEngine* eng, uint8_t* buf, int* width, int* height) {
    if (!eng || !buf) return false;
    
    if (width) *width = eng->frame_width;
    if (height) *height = eng->frame_height;
    
    // Copy frame buffer (mock data)
    memset(buf, 0, eng->frame_width * eng->frame_height * 4);
    eng->frame_valid = true;
    
    return eng->frame_valid;
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
// Version
// =============================================================================

const char* litt_version(void) {
    return "1.0.0";
}
