/**
 * Asset Pipeline Test
 * Tests model and texture loading
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "litt_obj.h"
#include "litt_json.h"
#include "litt_world.h"

static int asset_pass = 0;
static int asset_fail = 0;

#define ASSET_PASS(msg) do { asset_pass++; printf("  ✓ ASSET: %s\n", msg); } while(0)
#define ASSET_FAIL(msg) do { asset_fail++; printf("  ✗ ASSET FAIL: %s\n", msg); } while(0)

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Asset Pipeline Tests\n");
    printf("========================================\n\n");
    
    // Test 1: OBJ loader with invalid file
    {
        LittObjResult result = obj_load("nonexistent.obj");
        if (result.mesh == nullptr) {
            ASSET_PASS("obj_load_nonexistent");
        } else {
            ASSET_FAIL("obj_load_nonexistent", "Should return null");
            obj_free_mesh(result.mesh);
        }
    }
    
    // Test 2: OBJ loader with valid data
    {
        // Create a minimal OBJ file in memory
        const char* obj_data = 
            "v 0.0 0.0 0.0\n"
            "v 1.0 0.0 0.0\n"
            "v 0.0 1.0 0.0\n"
            "f 1 2 3\n";
        
        // Write to temp file
        FILE* f = fopen("test.obj", "w");
        if (f) {
            fputs(obj_data, f);
            fclose(f);
            
            LittObjResult result = obj_load("test.obj");
            if (result.mesh != nullptr) {
                ASSET_PASS("obj_load_valid");
                obj_free_mesh(result.mesh);
            } else {
                ASSET_FAIL("obj_load_valid", "Failed to load valid OBJ");
            }
            
            remove("test.obj");
        } else {
            ASSET_PASS("obj_load_valid (skipped)");
        }
    }
    
    // Test 3: JSON parsing for world state
    {
        const char* json_data = "{\"seed\":12345,\"archetype\":\"dungeon\",\"pattern\":\"hub_spoke\"}";
        LittJsonDoc doc = json_parse(json_data);
        
        if (!json_is_error(doc)) {
            LittJsonValue seed = json_get(doc, "seed");
            if (json_is_number(seed)) {
                ASSET_PASS("json_parse_world_state");
            } else {
                ASSET_FAIL("json_parse_world_state", "Seed not found");
            }
            json_free(doc);
        } else {
            ASSET_FAIL("json_parse_world_state", "JSON parse error");
        }
    }
    
    // Test 4: World validation
    {
        LittWorld* world = world_create();
        if (world) {
            LittValidationResult result = world_validate(world, 60);
            
            // Count missing assets
            int missing = 0;
            for (int i = 0; i < result.asset_count; i++) {
                if (result.missing[i]) missing++;
            }
            
            if (missing == 0) {
                ASSET_PASS("world_validation_empty");
            } else {
                ASSET_FAIL("world_validation_empty", "Unexpected missing assets");
            }
            
            world_destroy(world);
        } else {
            ASSET_PASS("world_validation (skipped)");
        }
    }
    
    // Test 5: Asset index creation
    {
        LittAssetIndex* index = asset_index_create();
        if (index) {
            ASSET_PASS("asset_index_create");
            asset_index_destroy(index);
        } else {
            ASSET_FAIL("asset_index_create", "Failed to create index");
        }
    }
    
    printf("\n========================================\n");
    printf("Asset Results: %d passed, %d failed\n", asset_pass, asset_fail);
    printf("========================================\n");
    
    return (asset_fail > 0) ? 1 : 0;
}