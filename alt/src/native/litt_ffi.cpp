// FFI Implementation for Dither3D
#include "littcore/litt_dither.h"
#include <cstring>
#include <cstdio>

namespace litt {

const char* litt_dither_texture_path(LittDitherPattern pattern, char* buf, size_t cap) {
    static const char* paths[] = {
        "assets/dither3d/Dither3D_1x1.png",
        "assets/dither3d/Dither3D_2x2.png",
        "assets/dither3d/Dither3D_4x4.png",
        "assets/dither3d/Dither3D_8x8.png"
    };
    int idx = static_cast<int>(pattern);
    if (idx < 0 || idx > 3) idx = 3;
    strncpy(buf, paths[idx], cap - 1);
    buf[cap - 1] = '\0';
    return buf;
}

const char* litt_dither_ramp_path(char* buf, size_t cap) {
    strncpy(buf, "assets/dither3d/Dither3D_8x8_Ramp.png", cap - 1);
    buf[cap - 1] = '\0';
    return buf;
}

void litt_dither_default_material(LittDitherColorMode mode, LittDitherMaterial* out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    out->enabled = 1;
    out->color_mode = static_cast<int>(mode);
    out->pattern = LITT_DITHER_P8x8;
    out->scale = 5.0f;
    out->contrast = 1.0f;
    out->stretch_smoothness = 1.0f;
    out->input_exposure = 1.0f;
}

} // namespace litt

extern "C" {

const char* litt_version() {
    return "1.0.0-dither3d";
}

// World deployment stubs (to be implemented)
LittWorld* litt_deploy_world(const char*, const char*, char* out_error) {
    if (out_error) {
        strncpy(out_error, "World deployment not yet implemented", 255);
        out_error[255] = '\0';
    }
    return nullptr;
}

size_t litt_world_triangles(const LittWorld*) { return 0; }
size_t litt_world_spheres(const LittWorld*) { return 0; }
size_t litt_world_meshes(const LittWorld*) { return 0; }

int litt_world_missing_count(const LittWorld*) { return 0; }
int litt_world_missing_at(const LittWorld*, int, char*, size_t) { return 0; }
void litt_world_free(LittWorld*) {}

} // extern "C"
