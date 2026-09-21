#include "litt_dither.h"
#include <cassert>
#include <cstdint>
#include <vector>

using namespace litt;

int main() {
    DitherAssetManager assets;
    assert(assets.ready());

    const auto& p1 = assets.get_texture(DitherPattern::P1x1);
    const auto& p8 = assets.get_texture(DitherPattern::P8x8);
    assert(p1.width == 16u && p1.height == 16u && p1.depth == 1u);
    assert(p1.data.size() == 16u * 16u);
    assert(p8.width == 128u && p8.height == 128u && p8.depth == 64u);
    assert(p8.data.size() == 128u * 128u * 64u);
    assert(assets.get_ramp().data.size() == 256u);

    std::vector<uint8_t> pixels = {
        0, 0, 0, 7,
        64, 64, 64, 8,
        192, 192, 192, 9,
        255, 255, 255, 10,
    };
    DitherMaterial material;
    material.enabled = true;
    material.pattern = DitherPattern::P2x2;
    material.color_mode = DitherColorMode::Grayscale;
    assert(DitherProcessor::apply_rgba8(pixels, 2, 2, material));
    for (size_t i = 0; i < pixels.size(); i += 4) {
        assert((pixels[i] == 0u || pixels[i] == 255u));
        assert(pixels[i] == pixels[i + 1] && pixels[i] == pixels[i + 2]);
    }
    assert(pixels[3] == 7u && pixels[7] == 8u && pixels[11] == 9u && pixels[15] == 10u);

    const auto unchanged = pixels;
    material.enabled = false;
    assert(DitherProcessor::apply_rgba8(pixels, 2, 2, material));
    assert(pixels == unchanged);

    material.enabled = true;
    material.contrast = 3.0f;
    assert(!DitherProcessor::apply_rgba8(pixels, 2, 2, material));
    assert(!DitherProcessor::apply_rgba8(pixels, 0, 2, material));

    return 0;
}
