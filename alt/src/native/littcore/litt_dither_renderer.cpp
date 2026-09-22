// Dither3D CPU post-process for the supported software renderer
#include "litt_dither.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace litt {

static uint32_t pattern_order(DitherPattern pattern) {
    switch (pattern) {
        case DitherPattern::P1x1: return 1u;
        case DitherPattern::P2x2: return 2u;
        case DitherPattern::P4x4: return 4u;
        case DitherPattern::P8x8: return 8u;
    }
    return 8u;
}

static std::vector<uint32_t> threshold_matrix(uint32_t order) {
    std::vector<uint32_t> matrix(1, 0);
    uint32_t n = 1;
    while (n < order) {
        const uint32_t next = n * 2u;
        std::vector<uint32_t> out(static_cast<size_t>(next) * next);
        for (uint32_t y = 0; y < n; ++y) {
            for (uint32_t x = 0; x < n; ++x) {
                const uint32_t v = matrix[static_cast<size_t>(y) * n + x] * 4u;
                out[static_cast<size_t>(y) * next + x] = v;
                out[static_cast<size_t>(y) * next + x + n] = v + 2u;
                out[static_cast<size_t>(y + n) * next + x] = v + 3u;
                out[static_cast<size_t>(y + n) * next + x + n] = v + 1u;
            }
        }
        matrix.swap(out);
        n = next;
    }
    return matrix;
}

static float adjust_channel(float value, const DitherMaterial& material) {
    value = value * material.input_exposure + material.input_offset;
    value = (value - 0.5f) * material.contrast + 0.5f;
    return std::clamp(value, 0.0f, 1.0f);
}

static uint8_t threshold_channel(float value, float threshold, bool inverse) {
    const bool on = inverse ? value < threshold : value >= threshold;
    return on ? 255u : 0u;
}

bool DitherProcessor::apply_rgba8(std::vector<uint8_t>& rgba, uint32_t width, uint32_t height,
                                  const DitherMaterial& material) {
    if (!material.enabled) return true;
    if (width == 0 || height == 0 ||
        static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / height) return false;
    const size_t pixels = static_cast<size_t>(width) * height;
    if (pixels > kMaxPixels || pixels > std::numeric_limits<size_t>::max() / 4u ||
        rgba.size() != pixels * 4u) return false;
    if (!std::isfinite(material.scale) || material.scale < 2.0f || material.scale > 10.0f ||
        !std::isfinite(material.contrast) || material.contrast < 0.0f || material.contrast > 2.0f ||
        !std::isfinite(material.input_exposure) || material.input_exposure < 0.0f ||
        material.input_exposure > 5.0f || !std::isfinite(material.input_offset) ||
        material.input_offset < -1.0f || material.input_offset > 1.0f) {
        return false;
    }

    const uint32_t order = pattern_order(material.pattern);
    const std::vector<uint32_t> matrix = threshold_matrix(order);
    const float denom = static_cast<float>(order * order);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t index = (static_cast<size_t>(y) * width + x) * 4u;
            const float base_threshold =
                (static_cast<float>(matrix[static_cast<size_t>(y % order) * order + (x % order)]) +
                 0.5f) / denom;

            float r = adjust_channel(static_cast<float>(rgba[index + 0]) / 255.0f, material);
            float g = adjust_channel(static_cast<float>(rgba[index + 1]) / 255.0f, material);
            float b = adjust_channel(static_cast<float>(rgba[index + 2]) / 255.0f, material);

            if (material.color_mode == DitherColorMode::Grayscale) {
                const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                const uint8_t value = threshold_channel(lum, base_threshold, material.inverse_dots);
                rgba[index + 0] = value;
                rgba[index + 1] = value;
                rgba[index + 2] = value;
            } else {
                const float tr = base_threshold;
                const float tg = std::fmod(base_threshold + 0.3333333f, 1.0f);
                const float tb = std::fmod(base_threshold + 0.6666667f, 1.0f);
                rgba[index + 0] = threshold_channel(r, tr, material.inverse_dots);
                rgba[index + 1] = threshold_channel(g, tg, material.inverse_dots);
                rgba[index + 2] = threshold_channel(b, tb, material.inverse_dots);
            }
        }
    }
    return true;
}

} // namespace litt
