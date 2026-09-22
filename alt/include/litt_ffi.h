// Litt Dither FFI - small C ABI for Dither3D configuration helpers
#ifndef LITT_FFI_H
#define LITT_FFI_H

#include <stddef.h>

#if defined(_WIN32)
  #if defined(LITT_FFI_BUILD)
    #define LITT_FFI_API __declspec(dllexport)
  #else
    #define LITT_FFI_API __declspec(dllimport)
  #endif
#else
  #define LITT_FFI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LITT_DITHER_FFI_ABI_VERSION_MAJOR 1u
#define LITT_DITHER_FFI_ABI_VERSION_MINOR 0u
#define LITT_DITHER_FFI_ABI_VERSION \
    ((LITT_DITHER_FFI_ABI_VERSION_MAJOR << 16) | LITT_DITHER_FFI_ABI_VERSION_MINOR)

typedef enum {
    LITT_DITHER_GRAYSCALE = 0,
    LITT_DITHER_RGB = 1,
    LITT_DITHER_CMYK = 2,
} LittDitherColorMode;

typedef enum {
    LITT_DITHER_P1x1 = 0,
    LITT_DITHER_P2x2 = 1,
    LITT_DITHER_P4x4 = 2,
    LITT_DITHER_P8x8 = 3,
} LittDitherPattern;

typedef struct {
    int enabled;
    int color_mode;
    int pattern;
    float scale;
    float size_variability;
    float contrast;
    float stretch_smoothness;
    float input_exposure;
    float input_offset;
    int inverse_dots;
    int radial_compensation;
    int quantize_layers;
    int debug_fractal;
} LittDitherMaterial;

LITT_FFI_API unsigned litt_dither_ffi_abi_version(void);
LITT_FFI_API const char* litt_dither_texture_path(
    LittDitherPattern pattern, char* buf, size_t cap);
LITT_FFI_API const char* litt_dither_ramp_path(char* buf, size_t cap);
LITT_FFI_API int litt_dither_default_material(
    LittDitherColorMode mode, LittDitherMaterial* out);

#ifdef __cplusplus
}
#endif

#endif
