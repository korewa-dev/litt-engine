// Dither3D - dependency-free generated patterns and CPU post-process
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace litt {

enum class DitherColorMode : uint32_t {
    Grayscale = 0,
    RGB = 1,
    CMYK = 2,
};

enum class DitherPattern : uint32_t {
    P1x1 = 0,
    P2x2 = 1,
    P4x4 = 2,
    P8x8 = 3,
};

struct DitherMaterial {
    bool enabled = false;
    DitherColorMode color_mode = DitherColorMode::Grayscale;
    DitherPattern pattern = DitherPattern::P8x8;
    float scale = 5.0f;
    float size_variability = 0.0f;
    float contrast = 1.0f;
    float stretch_smoothness = 1.0f;
    float input_exposure = 1.0f;
    float input_offset = 0.0f;
    bool inverse_dots = false;
    bool radial_compensation = false;
    bool quantize_layers = false;
    bool debug_fractal = false;

    struct Uniforms {
        float albedo_r = 1.0f;
        float albedo_g = 1.0f;
        float albedo_b = 1.0f;
        float metallic = 0.0f;
        float dither_enabled = 0.0f;
        float dither_scale = 5.0f;
        float dither_size_var = 0.0f;
        float dither_contrast = 1.0f;
        uint32_t dither_color_mode = 0;
        uint32_t dither_pattern = 3;
        float dither_input_exp = 1.0f;
        float dither_input_off = 0.0f;
    };

    Uniforms to_uniforms() const {
        Uniforms u{};
        u.dither_enabled = enabled ? 1.0f : 0.0f;
        u.dither_scale = scale;
        u.dither_size_var = size_variability;
        u.dither_contrast = contrast;
        u.dither_color_mode = static_cast<uint32_t>(color_mode);
        u.dither_pattern = static_cast<uint32_t>(pattern);
        u.dither_input_exp = input_exposure;
        u.dither_input_off = input_offset;
        return u;
    }
};

struct DitherTexture {
    std::string name;
    DitherPattern pattern = DitherPattern::P8x8;
    std::vector<uint8_t> data;
    uint32_t width = 128;
    uint32_t height = 128;
    uint32_t depth = 64;
};

struct DitherRampTexture {
    std::string name;
    std::vector<uint8_t> data;
    uint32_t width = 256;
    uint32_t height = 1;
};

class DitherAssetManager {
public:
    DitherAssetManager() { generate_textures(); }

    const DitherTexture& get_texture(DitherPattern pattern) const;
    const DitherRampTexture& get_ramp() const { return ramp_; }
    void generate_textures();
    bool ready() const { return textures_generated_; }

    static uint32_t get_texture_width(DitherPattern pattern);
    static uint32_t get_texture_depth(DitherPattern pattern);
    static uint32_t get_dots_per_side(DitherPattern pattern);

private:
    std::unordered_map<DitherPattern, DitherTexture> textures_;
    DitherRampTexture ramp_;
    bool textures_generated_ = false;
};

class DitherProcessor {
public:
    static constexpr size_t kMaxPixels = 16u * 1024u * 1024u;

    // Applies ordered dithering in-place to RGBA8 pixels. Alpha is preserved.
    static bool apply_rgba8(std::vector<uint8_t>& rgba, uint32_t width, uint32_t height,
                            const DitherMaterial& material);
};

} // namespace litt
