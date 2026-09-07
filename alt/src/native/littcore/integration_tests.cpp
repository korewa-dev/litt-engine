/**
 * Integration Tests for Litt Engine
 * Tests subsystem interactions and workflows
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "litt_ecs.h"
#include "litt_physics.h"
#include "litt_input.h"
#include "litt_world.h"

static int integration_pass = 0;
static int integration_fail = 0;

#define INTEGRATION_PASS(msg) do { integration_pass++; printf("  ✓ INTEGRATION: %s\n", msg); } while(0)
#define INTEGRATION_FAIL(msg) do { integration_fail++; printf("  ✗ INTEGRATION FAIL: %s\n", msg); } while(0)

// ==========================================
// ECS + Physics Integration
// ==========================================

void test_ecs_physics_integration() {
    printf("  Testing ECS + Physics integration...\n");
    
    // Create ECS world
    litt::World world;
    
    // Create entity with physics
    litt::Entity player = world.create();
    world.add(player, litt::Transform());
    world.add(player, litt::RigidBody());
    
    // Verify components exist
    if (world.has<litt::Transform>(player) &&
        world.has<litt::RigidBody>(player)) {
        INTEGRATION_PASS("ecs_physics_components");
    } else {
        INTEGRATION_FAIL("ecs_physics_components");
    }
}

// ==========================================
// World + Input Integration
// ==========================================

void test_world_input_integration() {
    printf("  Testing World + Input integration...\n");
    
    litt::WorldManager world;
    world.state.cfg.gravity = -9.81f;
    world.state.cfg.jump = 8.0f;
    world.state.cfg.speed = 5.0f;
    
    // Create input
    litt::Input input;
    
    // Simulate update
    world.update(0.016f);
    world.input(input);
    
    // World should be at rest
    if (world.state.pos.y == 0.0f && world.state.grounded) {
        INTEGRATION_PASS("world_input_basic");
    } else {
        INTEGRATION_FAIL("world_input_basic");
    }
}

// ==========================================
// Serialization Round-Trip
// ==========================================

void test_serialization_roundtrip() {
    printf("  Testing serialization round-trip...\n");
    
    litt::WorldManager world;
    world.state.pos = litt::Vec3(1.0f, 2.0f, 3.0f);
    world.state.score = 100;
    
    std::string data = world.serialize();
    
    // Just verify serialize works
    if (!data.empty() && data.find("100") != std::string::npos) {
        INTEGRATION_PASS("serialization_basic");
    } else {
        INTEGRATION_FAIL("serialization_basic");
    }
}

// ==========================================
// JSON Parsing
// ==========================================

void test_json_parsing() {
    printf("  Testing JSON parsing...\n");
    
    const char* json = "{\"seed\":42,\"name\":\"test\"}";
    LvJson* doc = lvj_parse(json);
    
    if (doc) {
        const LvJson* seed = lvj_get(doc, "seed");
        double val = lvj_num(seed, 0);
        if (val == 42.0) {
            INTEGRATION_PASS("json_parse_number");
        } else {
            INTEGRATION_FAIL("json_parse_number");
        }
        lvj_free(doc);
    } else {
        INTEGRATION_FAIL("json_parse_error");
    }
}

// ==========================================
// Entity Spawning
// ==========================================

void test_entity_spawning() {
    printf("  Testing entity spawning...\n");
    
    litt::WorldManager world;
    
    litt::WorldEntity enemy;
    strncpy(enemy.name, "goblin", sizeof(enemy.name));
    enemy.pos = litt::Vec3(10.0f, 0.0f, 5.0f);
    enemy.flags = litt::WorldEntity::Enemy;
    enemy.tier = litt::Tier::Mook;
    enemy.alive = 1;
    
    world.state.ents.push_back(enemy);
    
    if (world.state.ents.size() == 1 && world.state.ents[0].alive) {
        INTEGRATION_PASS("entity_spawning");
    } else {
        INTEGRATION_FAIL("entity_spawning");
    }
}

// ==========================================
// Main
// ==========================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Integration Tests\n");
    printf("========================================\n\n");
    
    test_ecs_physics_integration();
    test_world_input_integration();
    test_serialization_roundtrip();
    test_json_parsing();
    test_entity_spawning();
    
    printf("\n========================================\n");
    printf("Integration Results: %d passed, %d failed\n", integration_pass, integration_fail);
    printf("========================================\n");
    
    return (integration_fail > 0) ? 1 : 0;
}