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
#define ASSET_FAIL(msg, reason) do { asset_fail++; printf("  ✗ ASSET FAIL: %s - %s\n", msg, reason); } while(0)

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Asset Pipeline Tests\n");
    printf("========================================\n\n");
    
    // Test 1: OBJ loader with invalid file
    {
        LvModel model = {};
        int result = lv_obj_load("nonexistent.obj", &model);
        if (result != 0) {
            ASSET_PASS("obj_load_nonexistent");
        } else {
            ASSET_FAIL("obj_load_nonexistent", "Should return error");
            lv_model_free(&model);
        }
    }
    
    // Test 2: OBJ loader with valid data
    {
        const char* obj_data = 
            "v 0.0 0.0 0.0\n"
            "v 1.0 0.0 0.0\n"
            "v 0.0 1.0 0.0\n"
            "f 1 2 3\n";
        
        FILE* f = fopen("test.obj", "w");
        if (f) {
            fputs(obj_data, f);
            fclose(f);
            
            LvModel model = {};
            int result = lv_obj_load("test.obj", &model);
            if (result == 0 && model.count > 0) {
                ASSET_PASS("obj_load_valid");
                lv_model_free(&model);
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
        LvJson* doc = lvj_parse(json_data);
        
        if (doc) {
            const LvJson* seed = lvj_get(doc, "seed");
            double seed_val = lvj_num(seed, 0);
            if (seed_val == 12345.0) {
                ASSET_PASS("json_parse_world_state");
            } else {
                ASSET_FAIL("json_parse_world_state", "Seed not found");
            }
            lvj_free(doc);
        } else {
            ASSET_FAIL("json_parse_world_state", "JSON parse error");
        }
    }
    
    // Test 4: World manager basic test
    {
        litt::WorldManager world;
        if (world.state.cfg.gravity == -9.81f) {
            ASSET_PASS("world_manager_create");
        } else {
            ASSET_FAIL("world_manager_create", "Invalid config");
        }
    }
    
    // Test 5: JSON array parsing
    {
        const char* json_data = "{\"pos\":[1.0,2.0,3.0],\"score\":100}";
        LvJson* doc = lvj_parse(json_data);
        
        if (doc) {
            const LvJson* pos = lvj_get(doc, "pos");
            float p[3] = {0};
            if (lvj_arr_f3(pos, p) && p[0] == 1.0f && p[1] == 2.0f && p[2] == 3.0f) {
                ASSET_PASS("json_array_parse");
            } else {
                ASSET_FAIL("json_array_parse", "Array parse failed");
            }
            lvj_free(doc);
        } else {
            ASSET_FAIL("json_array_parse", "JSON parse error");
        }
    }
    
    printf("\n========================================\n");
    printf("Asset Results: %d passed, %d failed\n", asset_pass, asset_fail);
    printf("========================================\n");
    
    return (asset_fail > 0) ? 1 : 0;
}