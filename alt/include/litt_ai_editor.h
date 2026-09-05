// AI Editor API for Litt Engine
// This API allows any AI system to interact with the engine to build editors
// Standard JSON-based protocol, C++ backend, Python frontend

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Version
// =============================================================================
#define LITT_AI_EDITOR_VERSION_MAJOR 1
#define LITT_AI_EDITOR_VERSION_MINOR 0
#define LITT_AI_EDITOR_VERSION_PATCH 0

// =============================================================================
// Basic Types
// =============================================================================
typedef uint32_t litt_entity_t;
typedef uint32_t litt_component_t;
typedef void* litt_handle_t;

// =============================================================================
// Scene Management
// =============================================================================
typedef struct {
    float x, y, z;
} litt_vec3_t;

typedef struct {
    float r, g, b, a;
} litt_color_t;

typedef struct {
    litt_vec3_t position;
    litt_vec3_t rotation;  // Euler angles in degrees
    litt_vec3_t scale;
    litt_color_t color;
    char name[256];
} litt_entity_desc_t;

typedef struct {
    int success;
    litt_entity_t entity_id;
    char error[256];
} litt_create_entity_result_t;

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
    LITT_COMPONENT_AUDIO = 7
} litt_component_type_t;

// =============================================================================
// Callback Types (for async operations)
// =============================================================================
typedef void (*litt_result_callback_t)(int success, const char* result, void* user_data);
typedef void (*litt_progress_callback_t)(int progress, const char* message, void* user_data);

// =============================================================================
// Editor Handle
// =============================================================================
typedef struct litt_editor litt_editor_t;

// =============================================================================
// Initialization
// =============================================================================
LITT_API litt_editor_t* litt_editor_create(const char* config_path);
LITT_API void litt_editor_destroy(litt_editor_t* editor);
LITT_API const char* litt_editor_version(void);

// =============================================================================
// Scene Operations
// =============================================================================
LITT_API litt_create_entity_result_t litt_editor_create_entity(litt_editor_t* editor, 
                                                               const litt_entity_desc_t* desc);
LITT_API int litt_editor_delete_entity(litt_editor_t* editor, litt_entity_t entity_id);
LITT_API int litt_editor_set_position(litt_editor_t* editor, litt_entity_t entity_id, 
                                       litt_vec3_t position);
LITT_API int litt_editor_set_rotation(litt_editor_t* editor, litt_entity_t entity_id, 
                                       litt_vec3_t rotation);
LITT_API int litt_editor_set_scale(litt_editor_t* editor, litt_entity_t entity_id, 
                                    litt_vec3_t scale);
LITT_API int litt_editor_get_entity(litt_editor_t* editor, litt_entity_t entity_id,
                                     litt_entity_desc_t* out);
LITT_API int litt_editor_list_entities(litt_editor_t* editor, 
                                        litt_entity_t* out_ids, int* count, int max_count);

// =============================================================================
// Component Operations
// =============================================================================
LITT_API int litt_editor_add_component(litt_editor_t* editor, litt_entity_t entity_id,
                                        litt_component_type_t type, const char* json_config);
LITT_API int litt_editor_remove_component(litt_editor_t* editor, litt_entity_t entity_id,
                                           litt_component_type_t type);
LITT_API int litt_editor_set_component_property(litt_editor_t* editor, litt_entity_t entity_id,
                                                 litt_component_type_t type, 
                                                 const char* property, const char* value);

// =============================================================================
// Asset Operations
// =============================================================================
LITT_API int litt_editor_load_asset(litt_editor_t* editor, const char* asset_path,
                                     const char* asset_type, litt_entity_t* out_entity);
LITT_API int litt_editor_export_scene(litt_editor_t* editor, const char* output_path);
LITT_API int litt_editor_import_scene(litt_editor_t* editor, const char* input_path);

// =============================================================================
// Renderer Operations
// =============================================================================
LITT_API int litt_editor_render_frame(litt_editor_t* editor, int width, int height,
                                       const char* output_path);
LITT_API int litt_editor_set_camera(litt_editor_t* editor, const char* camera_config_json);
LITT_API int litt_editor_add_light(litt_editor_t* editor, const char* light_config_json);

// =============================================================================
// Physics Operations
// =============================================================================
LITT_API int litt_editor_add_physics(litt_editor_t* editor, litt_entity_t entity_id,
                                      const char* physics_config_json);
LITT_API int litt_editor_step_physics(litt_editor_t* editor, float dt);

// =============================================================================
// Query Operations
// =============================================================================
LITT_API int litt_editor_count_entities(litt_editor_t* editor);
LITT_API int litt_editor_count_components(litt_editor_t* editor, litt_component_type_t type);
LITT_API int litt_editor_find_by_name(litt_editor_t* editor, const char* name,
                                       litt_entity_t* out_id);
LITT_API int litt_editor_find_by_tag(litt_editor_t* editor, const char* tag,
                                      litt_entity_t* out_ids, int* count, int max_count);

// =============================================================================
// Script Operations
// =============================================================================
LITT_API int litt_editor_execute_script(litt_editor_t* editor, const char* script_path,
                                         const char* params_json, char* out_result, 
                                         int result_size);
LITT_API int litt_editor_evaluate_expression(litt_editor_t* editor, const char* expression,
                                              char* out_result, int result_size);

// =============================================================================
// Event System
// =============================================================================
LITT_API int litt_editor_subscribe_event(litt_editor_t* editor, const char* event_name,
                                          litt_result_callback_t callback, void* user_data);
LITT_API int litt_editor_unsubscribe_event(litt_editor_t* editor, const char* event_name);
LITT_API int litt_editor_emit_event(litt_editor_t* editor, const char* event_name,
                                     const char* data_json);

// =============================================================================
// Serialization
// =============================================================================
LITT_API char* litt_editor_serialize_scene(litt_editor_t* editor, int* out_size);
LITT_API int litt_editor_deserialize_scene(litt_editor_t* editor, const char* json, int size);

#ifdef __cplusplus
}
#endif
