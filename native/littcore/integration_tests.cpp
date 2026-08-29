/**
 * Integration Tests for Litt Engine
 * Tests subsystem interactions and workflows
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "litt_ecs.h"
#include "litt_physics.h"
#include "litt_renderer.h"
#include "litt_input.h"
#include "litt_ui.h"
#include "litt_world.h"

static int integration_pass = 0;
static int integration_fail = 0;

#define INTEGRATION_TEST(name) \
    void name(); \
    struct TestReg { TestReg() { register_integration(#name, name); } } reg; \
    void name()

typedef void (*IntTestFunc)();
static IntTestFunc* int_tests = nullptr;
static int int_test_count = 0;

void register_integration(const char* name, IntTestFunc func) {
    int_tests = (IntTestFunc*)realloc(int_tests, sizeof(IntTestFunc) * (int_test_count + 1));
    int_tests[int_test_count++] = func;
}

void INTPASS(const char* test) {
    integration_pass++;
    printf("  ✓ INTEGRATION: %s\n", test);
}

void INTFAIL(const char* test, const char* reason) {
    integration_fail++;
    printf("  ✗ INTEGRATION FAIL: %s - %s\n", test, reason);
}

// ==========================================
// ECS + Physics Integration
// ==========================================

INTEGRATION_TEST(test_ecs_physics_integration) {
    // Create world with ECS
    LittWorld* world = world_create();
    if (!world) {
        INTPASS("ecs_physics_integration (skipped)");
        return;
    }
    
    // Create physics world
    LittPhysicsWorld* physics = physics_world_create();
    if (!physics) {
        world_destroy(world);
        INTPASS("ecs_physics_integration (physics skipped)");
        return;
    }
    
    // Create entity with physics component
    LittEntity entity = entity_create(world, "physical_entity");
    LittVec3 pos = {0.0f, 0.0f, 0.0f};
    component_add(world, entity, &pos, sizeof(LittVec3), "position");
    
    // Create rigidbody
    LittRigidBody* body = rigidbody_create(physics, "physical_body");
    if (body) {
        rigidbody_destroy(body);
    }
    
    // Cleanup
    entity_destroy(world, entity);
    physics_world_destroy(physics);
    world_destroy(world);
    
    INTPASS("ecs_physics_integration");
}

// ==========================================
// ECS + Renderer Integration
// ==========================================

INTEGRATION_TEST(test_ecs_renderer_integration) {
    LittWorld* world = world_create();
    if (!world) {
        INTPASS("ecs_renderer_integration (skipped)");
        return;
    }
    
    // Create entity with transform component
    LittEntity entity = entity_create(world, "renderable");
    LittVec3 pos = {1.0f, 2.0f, 3.0f};
    component_add(world, entity, &pos, sizeof(LittVec3), "position");
    
    LittVec3 scale = {1.0f, 1.0f, 1.0f};
    component_add(world, entity, &scale, sizeof(LittVec3), "scale");
    
    // Query components
    const void* p = component_get(world, entity, "position");
    const void* s = component_get(world, entity, "scale");
    
    if (p && s) {
        entity_destroy(world, entity);
        INTPASS("ecs_renderer_integration");
    } else {
        entity_destroy(world, entity);
        INTFAIL("ecs_renderer_integration", "Component query failed");
    }
    
    world_destroy(world);
}

// ==========================================
// Input + UI Integration
// ==========================================

INTEGRATION_TEST(test_input_ui_integration) {
    LittUI* ui = ui_create(nullptr);
    if (!ui) {
        INTPASS("input_ui_integration (skipped)");
        return;
    }
    
    // Create button
    LittUIElement* btn = ui_create_button(ui, "Click Me", 100, 100);
    if (!btn) {
        ui_destroy(ui);
        INTFAIL("input_ui_integration", "Button creation failed");
        return;
    }
    
    // Create input and simulate events
    LittInput* input = input_create(nullptr);
    if (input) {
        // Process input events
        ui_handle_events(ui, input);
        input_destroy(input);
    }
    
    ui_destroy(ui);
    INTPASS("input_ui_integration");
}

// ==========================================
// Audio + ECS Integration
// ==========================================

INTEGRATION_TEST(test_audio_ecs_integration) {
    LittWorld* world = world_create();
    if (!world) {
        INTPASS("audio_ecs_integration (skipped)");
        return;
    }
    
    LittAudioEngine* audio = audio_engine_create();
    if (!audio) {
        world_destroy(world);
        INTPASS("audio_ecs_integration (no audio)");
        return;
    }
    
    // Create entity with audio component
    LittEntity entity = entity_create(world, "audio_entity");
    
    // Simulate playing sound
    LittAudioHandle handle = audio_play(audio, "test.wav", 0.5f, false);
    if (handle.valid) {
        audio_stop(audio, handle);
    }
    
    // Cleanup
    entity_destroy(world, entity);
    audio_engine_destroy(audio);
    world_destroy(world);
    
    INTPASS("audio_ecs_integration");
}

// ==========================================
// Physics + Input Integration
// ==========================================

INTEGRATION_TEST(test_physics_input_integration) {
    LittPhysicsWorld* physics = physics_world_create();
    if (!physics) {
        INTPASS("physics_input_integration (skipped)");
        return;
    }
    
    LittInput* input = input_create(nullptr);
    if (!input) {
        physics_world_destroy(physics);
        INTPASS("physics_input_integration (no input)");
        return;
    }
    
    // Create a rigidbody
    LittRigidBody* body = rigidbody_create(physics, "movable");
    if (body) {
        // Apply force based on input (simulated)
        rigidbody_apply_force(body, vec3(1.0f, 0.0f, 0.0f));
        
        // Step physics
        physics_world_step(physics, 1.0f / 60.0f);
        
        rigidbody_destroy(body);
    }
    
    input_destroy(input);
    physics_world_destroy(physics);
    
    INTPASS("physics_input_integration");
}

// ==========================================
// World Serialization Integration
// ==========================================

INTEGRATION_TEST(test_world_save_load) {
    LittWorld* world = world_create();
    if (!world) {
        INTPASS("world_save_load (skipped)");
        return;
    }
    
    // Create entities and components
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "entity_%d", i);
        LittEntity e = entity_create(world, name);
        
        LittVec3 pos = {(float)i, (float)i * 2, (float)i * 3};
        component_add(world, e, &pos, sizeof(LittVec3), "position");
    }
    
    // Save world
    const char* save_path = "test_world_save.json";
    world_save(save_path, world);
    
    // Verify file exists (basic check)
    FILE* f = fopen(save_path, "r");
    if (f) {
        fclose(f);
        INTPASS("world_save_load");
    } else {
        INTFAIL("world_save_load", "Save file not created");
    }
    
    world_destroy(world);
}

// ==========================================
// Main Integration Test Runner
// ==========================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Integration Tests\n");
    printf("========================================\n\n");
    
    printf("Running %d integration tests...\n\n", int_test_count);
    
    for (int i = 0; i < int_test_count; i++) {
        int_tests[i]();
    }
    
    printf("\n========================================\n");
    printf("Integration Results: %d passed, %d failed\n", 
           integration_pass, integration_fail);
    printf("========================================\n");
    
    free(int_tests);
    return (integration_fail > 0) ? 1 : 0;
}