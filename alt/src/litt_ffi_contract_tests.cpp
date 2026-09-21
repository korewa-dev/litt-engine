#include "../include/litt_ffi.h"
#include <cassert>
#include <cstring>

int main() {
    assert(litt_dither_ffi_abi_version() == LITT_DITHER_FFI_ABI_VERSION);

    char path[128]{};
    assert(litt_dither_texture_path(LITT_DITHER_P1x1, path, sizeof(path)));
    assert(std::strstr(path, "Dither3D_1x1.png") != nullptr);
    assert(litt_dither_texture_path(LITT_DITHER_P8x8, path, sizeof(path)));
    assert(std::strstr(path, "Dither3D_8x8.png") != nullptr);
    assert(!litt_dither_texture_path(static_cast<LittDitherPattern>(99), path, sizeof(path)));

    char tiny[2]{};
    assert(!litt_dither_ramp_path(tiny, sizeof(tiny)));
    assert(litt_dither_ramp_path(path, sizeof(path)));

    LittDitherMaterial material{};
    assert(litt_dither_default_material(LITT_DITHER_RGB, &material));
    assert(material.enabled == 1);
    assert(material.color_mode == LITT_DITHER_RGB);
    assert(material.pattern == LITT_DITHER_P8x8);
    assert(material.scale == 5.0f);
    assert(!litt_dither_default_material(static_cast<LittDitherColorMode>(99), &material));
    assert(!litt_dither_default_material(LITT_DITHER_RGB, nullptr));
    return 0;
}
