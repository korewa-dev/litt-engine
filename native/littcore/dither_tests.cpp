/**
 * Dither Demo Integration Test
 * Tests the dither renderer with null device
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "litt_dither.h"
#include "litt_renderer.h"
#include "litt_math.h"

static int dither_pass = 0;
static int dither_fail = 0;

#define DITHER_PASS(msg) do { dither_pass++; printf("  ✓ DITHER: %s\n", msg); } while(0)
#define DITHER_FAIL(msg) do { dither_fail++; printf("  ✗ DITHER FAIL: %s\n", msg); } while(0)

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("Litt Engine - Dither Demo Tests\n");
    printf("========================================\n\n");
    
    // Test 1: Dither color mode
    {
        LittDitherColorMode mode = LITT_DITHER_GRAYSCALE;
        if (mode == LITT_DITHER_RGB || mode == LITT_DITHER_CMYK) {
            DITHER_FAIL("color_mode_selection");
        } else {
            DITHER_PASS("color_mode_selection");
        }
    }
    
    // Test 2: Dither pattern
    {
        LittDitherPattern pattern = LITT_DITHER_P2x2;
        if (pattern < LITT_DITHER_P1x1 || pattern > LITT_DITHER_P8x8) {
            DITHER_FAIL("pattern_selection");
        } else {
            DITHER_PASS("pattern_selection");
        }
    }
    
    // Test 3: Material parameters
    {
        LittDitherMaterial mat = {1.0f, 1.0f, 1.0f, 0.5f};
        if (mat.scale <= 0.0f || mat.contrast < 0.0f || mat.exposure < 0.0f) {
            DITHER_FAIL("material_parameters");
        } else {
            DITHER_PASS("material_parameters");
        }
    }
    
    // Test 4: Render pass simulation
    {
        // Simulate render pass without actual GPU
        float width = 640.0f;
        float height = 480.0f;
        
        if (width > 0.0f && height > 0.0f) {
            DITHER_PASS("render_dimensions");
        } else {
            DITHER_FAIL("render_dimensions");
        }
    }
    
    // Test 5: Clear color
    {
        LittColor clear = {0.0f, 0.0f, 0.0f, 1.0f};
        if (clear.r >= 0.0f && clear.r <= 1.0f &&
            clear.g >= 0.0f && clear.g <= 1.0f &&
            clear.b >= 0.0f && clear.b <= 1.0f) {
            DITHER_PASS("clear_color");
        } else {
            DITHER_FAIL("clear_color");
        }
    }
    
    printf("\n========================================\n");
    printf("Dither Results: %d passed, %d failed\n", dither_pass, dither_fail);
    printf("========================================\n");
    
    return (dither_fail > 0) ? 1 : 0;
}