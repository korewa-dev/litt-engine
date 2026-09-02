// C contract for the Litt engine FFI bridge.
// Any C++ engine can link litt_ffi.dll and deploy generated worlds natively.
#ifndef LITT_FFI_H
#define LITT_FFI_H

#include <cstddef>

#ifdef _WIN32
#define LITT_API extern "C" __declspec(dllimport)
#else
#define LITT_API extern "C"
#endif

// =============================================================================
// Basic Types
// =============================================================================

struct LittWorld;

// =============================================================================
// Version
// =============================================================================

LITT_API const char* litt_version();

// =============================================================================
// World Deployment
// =============================================================================

LITT_API LittWorld* litt_deploy_world(const char* scene_path,
                                      const char* assets_base,
                                      char* out_error /* >=256 bytes, nullable */);

LITT_API size_t litt_world_triangles(const LittWorld*);
LITT_API size_t litt_world_spheres(const LittWorld*);
LITT_API size_t litt_world_meshes(const LittWorld*);

LITT_API int  litt_world_missing_count(const LittWorld*);
LITT_API int  litt_world_missing_at(const LittWorld*, int index,
                                    char* buf, size_t cap);

LITT_API void litt_world_free(LittWorld*);

// =============================================================================
// Dither3D Support
// =============================================================================

// Dither color mode
typedef enum {
    LITT_DITHER_GRAYSCALE = 0,
    LITT_DITHER_RGB       = 1,
    LITT_DITHER_CMYK      = 2,
} LittDitherColorMode;

// Dither pattern size
typedef enum {
    LITT_DITHER_P1x1 = 0,
    LITT_DITHER_P2x2 = 1,
    LITT_DITHER_P4x4 = 2,
    LITT_DITHER_P8x8 = 3,
} LittDitherPattern;

// Dither material configuration
typedef struct {
    int    enabled;               // 0 or 1
    int    color_mode;            // LittDitherColorMode
    int    pattern;               // LittDitherPattern
    float  scale;                 // Dot scale (2.0-10.0)
    float  size_variability;      // 0=Bayer, 1=Halftone
    float  contrast;              // 0.0-2.0
    float  stretch_smoothness;    // 0.0-2.0
    float  input_exposure;        // 0.0-5.0
    float  input_offset;          // -1.0-1.0
    int    inverse_dots;          // 0 or 1
    int    radial_compensation;   // 0 or 1
    int    quantize_layers;       // 0 or 1
    int    debug_fractal;         // 0 or 1
} LittDitherMaterial;

// Get dither texture path by pattern
LITT_API const char* litt_dither_texture_path(LittDitherPattern pattern, char* buf, size_t cap);
LITT_API const char* litt_dither_ramp_path(char* buf, size_t cap);

// Get default dither material for a mode
LITT_API void litt_dither_default_material(LittDitherColorMode mode, LittDitherMaterial* out);

#endif // LITT_FFI_H
