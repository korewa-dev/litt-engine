// Litt C API - C bindings for Litt Engine
// This file provides a C-compatible interface for the GUI to use
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Basic Types
// =============================================================================

typedef uint32_t litt_entity_t;
typedef uint32_t litt_component_t;

typedef struct {
    float x, y, z;
} litt_vec3_t;

typedef struct {
    float r, g, b, a;
} litt_color_t;

// =============================================================================
// Engine State
// =============================================================================

typedef enum {
    LITT_ENGINE_STATE_DISCONNECTED = 0,
    LITT_ENGINE_STATE_CONNECTING,
    LITT_ENGINE_STATE_CONNECTED,
    LITT_ENGINE_STATE_RUNNING,
    LITT_ENGINE_STATE_PAUSED,
    LITT_ENGINE_STATE_ERROR
} LittEngineState;

// =============================================================================
// Quality Presets
// =============================================================================

typedef enum {
    LITT_QUALITY_ULTRA_LOW = 0,
    LITT_QUALITY_LOW,
    LITT_QUALITY_MEDIUM,
    LITT_QUALITY_HIGH,
    LITT_QUALITY_ULTRA,
    LITT_QUALITY_ULTRA_MAX
} LittQualityPreset;

// =============================================================================
// GPU Info
// =============================================================================

typedef struct {
    char name[128];
    char vendor[64];
    uint64_t memory_total;
    uint64_t memory_free;
    uint32_t max_bounces;
    uint32_t max_texture_size;
} LittGpuInfo;

// =============================================================================
// Camera
// =============================================================================

typedef struct {
    float pos_x, pos_y, pos_z;
    float yaw, pitch;
    float fov;
    float exposure;
    float aspect_ratio;
} LittCamera;

// =============================================================================
// Render Stats
// =============================================================================

typedef struct {
    float fps;
    float frame_time_ms;
    float path_time_ms;
    uint32_t spp;
    uint32_t bounces;
    uint32_t width;
    uint32_t height;
} LittRenderStats;

// =============================================================================
// Entity Description
// =============================================================================

typedef struct {
    litt_vec3_t position;
    litt_vec3_t rotation;
    litt_vec3_t scale;
    litt_color_t color;
    char name[256];
} LittEntityDesc;

// =============================================================================
// Component Types
// =============================================================================

typedef enum {
    LITT_COMPONENT_TRANSFORM = 1,
    LITT_COMPONENT_MESH = 2,
    LITT_COMPONENT_PHYSICS = 3,
    LITT_COMPONENT_LIGHT = 4,
    LITT_COMPONENT_CAMERA = 5,
    LITT_COMPONENT_SCRIPT = 6,
    LITT_COMPONENT_AUDIO = 7,
    LITT_COMPONENT_UI = 8
} LittComponentType;

// =============================================================================
// Opaque Handle
// =============================================================================

typedef struct LittEngine LittEngine;
typedef struct LittWorld LittWorld;

// =============================================================================
// Logging Callback
// =============================================================================

typedef void (*LittLogCb)(const char* msg, void* user);

// =============================================================================
// Engine Creation/Destroy
// =============================================================================

LittEngine* litt_engine_create(void);
void litt_engine_destroy(LittEngine* eng);

// =============================================================================
// Engine State
// =============================================================================

LittEngineState litt_get_state(LittEngine* eng);
const char* litt_state_name(LittEngineState state);

// =============================================================================
// Connection
// =============================================================================

bool litt_connect(LittEngine* eng, const char* host, int port);
void litt_disconnect(LittEngine* eng);
bool litt_is_connected(LittEngine* eng);

// =============================================================================
// World Management
// =============================================================================

LittWorld* litt_world_create(const char* scene_path, const char* assets_base);
void litt_world_destroy(LittWorld* world);
bool litt_world_load(LittWorld* world, const char* scene_path);
bool litt_world_save(LittWorld* world, const char* scene_path);

// =============================================================================
// Entity Management
// =============================================================================

litt_entity_t litt_world_create_entity(LittWorld* world, const LittEntityDesc* desc);
bool litt_world_delete_entity(LittWorld* world, litt_entity_t entity_id);
bool litt_world_get_entity(LittWorld* world, litt_entity_t entity_id, LittEntityDesc* out);
int litt_world_list_entities(LittWorld* world, litt_entity_t* ids, int max_count);

// =============================================================================
// Component Management
// =============================================================================

bool litt_world_add_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type, const char* config_json);
bool litt_world_remove_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type);
bool litt_world_has_component(LittWorld* world, litt_entity_t entity_id, LittComponentType type);

// =============================================================================
// Transform Operations
// =============================================================================

bool litt_world_set_position(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* pos);
bool litt_world_get_position(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out);
bool litt_world_set_rotation(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* rot);
bool litt_world_get_rotation(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out);
bool litt_world_set_scale(LittWorld* world, litt_entity_t entity_id, const litt_vec3_t* scale);
bool litt_world_get_scale(LittWorld* world, litt_entity_t entity_id, litt_vec3_t* out);

// =============================================================================
// Simulation
// =============================================================================

bool litt_world_start(LittWorld* world);
bool litt_world_stop(LittWorld* world);
bool litt_world_is_running(LittWorld* world);
void litt_world_step(LittWorld* world, float dt);

// =============================================================================
// Rendering
// =============================================================================

void litt_engine_set_quality(LittEngine* eng, LittQualityPreset quality);
LittQualityPreset litt_engine_get_quality(LittEngine* eng);
void litt_engine_get_gpu_info(LittEngine* eng, LittGpuInfo* info);
void litt_engine_get_render_stats(LittEngine* eng, LittRenderStats* stats);
void litt_engine_get_camera(LittEngine* eng, LittCamera* cam);
void litt_engine_set_camera(LittEngine* eng, const LittCamera* cam);
bool litt_engine_get_framebuffer(LittEngine* eng, uint8_t* buf, int* width, int* height);

// =============================================================================
// Logging
// =============================================================================

void litt_engine_set_log_callback(LittEngine* eng, LittLogCb cb, void* user);
void litt_engine_log(LittEngine* eng, const char* fmt, ...);

// =============================================================================
// Version
// =============================================================================

const char* litt_version(void);

#ifdef __cplusplus
}
#endif
