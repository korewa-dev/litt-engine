// Litt Dither FFI implementation
#define LITT_FFI_BUILD
#include "../../include/litt_ffi.h"

#include <cstdio>
#include <cstring>

extern "C" {

unsigned litt_dither_ffi_abi_version(void) {
    return LITT_DITHER_FFI_ABI_VERSION;
}

const char* litt_dither_texture_path(
    LittDitherPattern pattern, char* buf, size_t cap) {
    static const char* paths[] = {
        "assets/dither3d/Dither3D_1x1.png",
        "assets/dither3d/Dither3D_2x2.png",
        "assets/dither3d/Dither3D_4x4.png",
        "assets/dither3d/Dither3D_8x8.png"
    };
    const int index = static_cast<int>(pattern);
    if (!buf || cap == 0 || index < 0 || index >= 4) return nullptr;
    const int written = std::snprintf(buf, cap, "%s", paths[index]);
    return written >= 0 && static_cast<size_t>(written) < cap ? buf : nullptr;
}

const char* litt_dither_ramp_path(char* buf, size_t cap) {
    if (!buf || cap == 0) return nullptr;
    static const char* path = "assets/dither3d/Dither3D_8x8_Ramp.png";
    const int written = std::snprintf(buf, cap, "%s", path);
    return written >= 0 && static_cast<size_t>(written) < cap ? buf : nullptr;
}

int litt_dither_default_material(
    LittDitherColorMode mode, LittDitherMaterial* out) {
    if (!out || mode < LITT_DITHER_GRAYSCALE || mode > LITT_DITHER_CMYK) return 0;
    std::memset(out, 0, sizeof(*out));
    out->enabled = 1;
    out->color_mode = static_cast<int>(mode);
    out->pattern = LITT_DITHER_P8x8;
    out->scale = 5.0f;
    out->contrast = 1.0f;
    out->stretch_smoothness = 1.0f;
    out->input_exposure = 1.0f;
    return 1;
}

} // extern "C"
