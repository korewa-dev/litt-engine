// Dither3D generated texture implementation
#include "litt_dither.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace litt {

uint32_t DitherAssetManager::get_texture_width(DitherPattern pattern) {
    switch (pattern) {
        case DitherPattern::P1x1: return 16;
        case DitherPattern::P2x2: return 32;
        case DitherPattern::P4x4: return 64;
        case DitherPattern::P8x8: return 128;
    }
    return 128;
}

uint32_t DitherAssetManager::get_texture_depth(DitherPattern pattern) {
    switch (pattern) {
        case DitherPattern::P1x1: return 1;
        case DitherPattern::P2x2: return 4;
        case DitherPattern::P4x4: return 16;
        case DitherPattern::P8x8: return 64;
    }
    return 64;
}

uint32_t DitherAssetManager::get_dots_per_side(DitherPattern pattern) {
    return get_texture_width(pattern) / 16u;
}

const DitherTexture& DitherAssetManager::get_texture(DitherPattern pattern) const {
    const auto it = textures_.find(pattern);
    if (it != textures_.end()) return it->second;
    const auto fallback = textures_.find(DitherPattern::P8x8);
    if (fallback == textures_.end()) throw std::logic_error("dither textures not generated");
    return fallback->second;
}

static std::vector<uint32_t> make_bayer(uint32_t size) {
    std::vector<uint32_t> matrix(1, 0);
    uint32_t n = 1;
    while (n < size) {
        const uint32_t next = n * 2u;
        std::vector<uint32_t> expanded(static_cast<size_t>(next) * next);
        for (uint32_t y = 0; y < n; ++y) {
            for (uint32_t x = 0; x < n; ++x) {
                const uint32_t base = matrix[static_cast<size_t>(y) * n + x] * 4u;
                expanded[static_cast<size_t>(y) * next + x] = base;
                expanded[static_cast<size_t>(y) * next + (x + n)] = base + 2u;
                expanded[static_cast<size_t>(y + n) * next + x] = base + 3u;
                expanded[static_cast<size_t>(y + n) * next + (x + n)] = base + 1u;
            }
        }
        matrix.swap(expanded);
        n = next;
    }
    return matrix;
}

void DitherAssetManager::generate_textures() {
    if (textures_generated_) return;
    for (DitherPattern pattern : {DitherPattern::P1x1, DitherPattern::P2x2,
                                  DitherPattern::P4x4, DitherPattern::P8x8}) {
        DitherTexture texture;
        texture.pattern = pattern;
        texture.width = get_texture_width(pattern);
        texture.height = texture.width;
        texture.depth = get_texture_depth(pattern);
        texture.name = "generated-" + std::to_string(texture.width);
        const size_t layer_pixels = static_cast<size_t>(texture.width) * texture.height;
        texture.data.resize(layer_pixels * texture.depth);

        const uint32_t order = std::max(1u, get_dots_per_side(pattern));
        const std::vector<uint32_t> bayer = make_bayer(order);
        const float denom = static_cast<float>(order * order);
        for (uint32_t z = 0; z < texture.depth; ++z) {
            const float depth_bias = texture.depth > 1
                ? static_cast<float>(z) / static_cast<float>(texture.depth - 1u)
                : 0.5f;
            for (uint32_t y = 0; y < texture.height; ++y) {
                for (uint32_t x = 0; x < texture.width; ++x) {
                    const uint32_t bx = x % order;
                    const uint32_t by = y % order;
                    const float threshold =
                        (static_cast<float>(bayer[static_cast<size_t>(by) * order + bx]) + 0.5f) /
                        denom;
                    const float value = std::clamp(0.5f * threshold + 0.5f * depth_bias, 0.0f, 1.0f);
                    const size_t index = static_cast<size_t>(z) * layer_pixels +
                                         static_cast<size_t>(y) * texture.width + x;
                    texture.data[index] = static_cast<uint8_t>(value * 255.0f + 0.5f);
                }
            }
        }
        textures_[pattern] = std::move(texture);
    }

    ramp_.name = "generated-ramp";
    ramp_.width = 256;
    ramp_.height = 1;
    ramp_.data.resize(256);
    for (uint32_t i = 0; i < 256; ++i) {
        const float t = static_cast<float>(i) / 255.0f;
        const float curved = t * t * (3.0f - 2.0f * t);
        ramp_.data[i] = static_cast<uint8_t>(curved * 255.0f + 0.5f);
    }
    textures_generated_ = true;
}

} // namespace litt
