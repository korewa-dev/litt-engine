// Texture layout explanation for Dither3D PNG files
//
// Dither3D PNG files are 2D representations of 3D textures.
// The Unity engine creates 3D textures from these PNGs using the Texture3D API.
//
// Texture Layout:
// ===============
// PNG dimensions: Width = 16 * dotsPerSide, Height = Width * layers
//
// Example for 8x8 pattern:
//   - PNG size: 128 x 8192 pixels (128 * 64 layers)
//   - 3D texture: 128 x 128 x 64 (width x height x depth)
//   - Each layer is a 128x128 row of pixels
//   - Layers are stacked vertically in the PNG
//
// Example for 4x4 pattern:
//   - PNG size: 64 x 4096 pixels (64 * 64 layers)
//   - 3D texture: 64 x 64 x 16
//
// Example for 2x2 pattern:
//   - PNG size: 32 x 512 pixels (32 * 16 layers)
//   - 3D texture: 32 x 32 x 4
//
// Example for 1x1 pattern:
//   - PNG size: 16 x 64 pixels (16 * 4 layers)
//   - 3D texture: 16 x 16 x 1
//
// Loading for Vulkan:
// ===================
// 1. Read PNG as 2D texture
// 2. Reinterpret pixel data as 3D texture:
//    - For each layer z (0 to depth-1):
//      - Read row z * width to (z+1) * width from PNG
//      - This becomes layer z of the 3D texture
//
// Shader expectations:
// ===================
// The shader uses:
//   float xRes = uDitherTexWidth;  // = PNG width
//   float dotsPerSide = xRes / 16.0;
//   float dotsTotal = dotsPerSide * dotsPerSide;  // = depth
//
// So for 8x8: xRes=128, dotsPerSide=8, dotsTotal=64
// The shader then samples: texture(uDitherTex, vec3(uv, subLayer))
// where subLayer is in [0, 1] normalized to the depth

#pragma once
#ifndef LITT_DITHER_LAYOUT_H
#define LITT_DITHER_LAYOUT_H

#include <cstdint>
#include <array>

namespace litt {

// Dither3D texture layout constants
namespace DitherLayout {

// Pattern dimensions
constexpr uint32_t PATTERN_1x1_WIDTH = 16;
constexpr uint32_t PATTERN_1x1_DEPTH = 1;
constexpr uint32_t PATTERN_1x1_PNG_HEIGHT = PATTERN_1x1_WIDTH * PATTERN_1x1_DEPTH;  // 16

constexpr uint32_t PATTERN_2x2_WIDTH = 32;
constexpr uint32_t PATTERN_2x2_DEPTH = 4;
constexpr uint32_t PATTERN_2x2_PNG_HEIGHT = PATTERN_2x2_WIDTH * PATTERN_2x2_DEPTH;  // 128

constexpr uint32_t PATTERN_4x4_WIDTH = 64;
constexpr uint32_t PATTERN_4x4_DEPTH = 16;
constexpr uint32_t PATTERN_4x4_PNG_HEIGHT = PATTERN_4x4_WIDTH * PATTERN_4x4_DEPTH;  // 1024

constexpr uint32_t PATTERN_8x8_WIDTH = 128;
constexpr uint32_t PATTERN_8x8_DEPTH = 64;
constexpr uint32_t PATTERN_8x8_PNG_HEIGHT = PATTERN_8x8_WIDTH * PATTERN_8x8_DEPTH;  // 8192

// Get PNG dimensions for a pattern
inline uint32_t GetPngWidth(uint32_t pattern) {
    switch (pattern) {
        case 0: return PATTERN_1x1_WIDTH;
        case 1: return PATTERN_2x2_WIDTH;
        case 2: return PATTERN_4x4_WIDTH;
        case 3: return PATTERN_8x8_WIDTH;
        default: return PATTERN_8x8_WIDTH;
    }
}

inline uint32_t GetPngHeight(uint32_t pattern) {
    switch (pattern) {
        case 0: return PATTERN_1x1_PNG_HEIGHT;
        case 1: return PATTERN_2x2_PNG_HEIGHT;
        case 2: return PATTERN_4x4_PNG_HEIGHT;
        case 3: return PATTERN_8x8_PNG_HEIGHT;
        default: return PATTERN_8x8_PNG_HEIGHT;
    }
}

inline uint32_t GetTextureDepth(uint32_t pattern) {
    switch (pattern) {
        case 0: return PATTERN_1x1_DEPTH;
        case 1: return PATTERN_2x2_DEPTH;
        case 2: return PATTERN_4x4_DEPTH;
        case 3: return PATTERN_8x8_DEPTH;
        default: return PATTERN_8x8_DEPTH;
    }
}

} // namespace DitherLayout
} // namespace litt

#endif // LITT_DITHER_LAYOUT_H
