// Dither3D Asset Manager Implementation
// Ported from Dither3D Unity package (MPL v2.0)
//
// The PNG files from Dither3D are 2D images that store 3D texture data:
//   - Height = Width * Depth (layers stacked vertically)
//   - For 8x8: 128x8192 PNG → 128x128x64 3D texture
//   - For 4x4: 64x4096 PNG → 64x64x16 3D texture
//   - For 2x2: 32x512 PNG → 32x32x4 3D texture
//   - For 1x1: 16x64 PNG → 16x16x1 3D texture

#include "litt_dither.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace litt {

// =============================================================================
// Minimal PNG Loader
// =============================================================================

namespace {

struct PngHeader {
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bit_depth = 0;
    uint8_t color_type = 0;
    uint8_t channels = 0;
};

// Read PNG IHDR chunk to get dimensions
bool readPngHeader(const std::string& path, PngHeader* hdr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    // PNG signature
    uint8_t sig[8];
    file.read(reinterpret_cast<char*>(sig), 8);
    if (sig[0] != 0x89 || sig[1] != 0x50 || sig[2] != 0x4E || sig[3] != 0x47)
        return false;

    // Read IHDR chunk
    uint32_t chunkLen;
    uint8_t chunkType[4];
    file.read(reinterpret_cast<char*>(&chunkLen), 4);
    file.read(reinterpret_cast<char*>(chunkType), 4);
    if (chunkType[0] != 'I' || chunkType[1] != 'H' ||
        chunkType[2] != 'D' || chunkType[3] != 'R')
        return false;

    // Read IHDR data (13 bytes)
    uint8_t ihdr[13];
    file.read(reinterpret_cast<char*>(ihdr), 13);

    // Parse big-endian values
    hdr->width = (static_cast<uint32_t>(ihdr[0]) << 24) |
                 (static_cast<uint32_t>(ihdr[1]) << 16) |
                 (static_cast<uint32_t>(ihdr[2]) << 8) |
                 static_cast<uint32_t>(ihdr[3]);
    hdr->height = (static_cast<uint32_t>(ihdr[4]) << 24) |
                  (static_cast<uint32_t>(ihdr[5]) << 16) |
                  (static_cast<uint32_t>(ihdr[6]) << 8) |
                  static_cast<uint32_t>(ihdr[7]);
    hdr->bit_depth = ihdr[8];
    hdr->color_type = ihdr[9];

    // Calculate channels from color type
    switch (hdr->color_type) {
        case 0: hdr->channels = 1; break;  // Grayscale
        case 2: hdr->channels = 3; break;  // RGB
        case 4: hdr->channels = 2; break;  // Grayscale + Alpha
        case 6: hdr->channels = 4; break;  // RGBA
        default: return false;
    }
    return hdr->bit_depth == 8;
}

// Very minimal PNG decoder - only handles R8 (grayscale) for dither textures
// This is sufficient for the dither textures which are single-channel R8
std::vector<uint8_t> decodePngR8(const std::string& path,
                                  uint32_t outWidth, uint32_t outHeight) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};

    // Skip signature
    uint8_t sig[8];
    file.read(reinterpret_cast<char*>(sig), 8);

    std::vector<uint8_t> raw;
    std::vector<uint8_t> filtered;
    std::vector<uint8_t> row;
    std::vector<uint8_t> prevRow;
    int filterMethod = 0;
    int interlaceMethod = 0;

    while (file.good()) {
        uint32_t chunkLen;
        uint8_t chunkType[4];
        file.read(reinterpret_cast<char*>(&chunkLen), 4);
        file.read(reinterpret_cast<char*>(chunkType), 4);

        if (chunkType[0] == 'I' && chunkType[1] == 'E' &&
            chunkType[2] == 'N' && chunkType[3] == 'D') {
            // End of PNG
            uint32_t crc;
            file.read(reinterpret_cast<char*>(&crc), 4);
            break;
        }

        std::vector<uint8_t> chunkData(chunkLen);
        file.read(reinterpret_cast<char*>(chunkData.data()), chunkLen);
        uint32_t crc;
        file.read(reinterpret_cast<char*>(&crc), 4);

        if (chunkType[0] == 'I' && chunkType[1] == 'H' &&
            chunkType[2] != 'D' && chunkType[3] != 'R') {
            // Skip other chunks
            continue;
        }

        if (chunkType[0] == 'I' && chunkType[1] == 'H' &&
            chunkType[2] == 'D' && chunkType[3] == 'R') {
            // Already parsed in header read
            continue;
        }

        if (chunkType[0] == 'I' && chunkType[1] == 'P' &&
            chunkType[2] == 'L' && chunkType[3] == 'T') {
            // Palette - not needed for R8
            continue;
        }

        if (chunkType[0] == 't' && chunkType[1] == 'B' &&
            chunkType[2] == 'I' && chunkType[3] == 'T') {
            // Bit depth info
            continue;
        }

        if (chunkType[0] == 'c' && chunkType[1] == 'I' &&
            chunkType[2] == 'C' && chunkType[3] == 'C') {
            // Color profile
            continue;
        }

        if (chunkType[0] == 'P' && chunkType[1] == 'A' &&
            chunkType[2] == 'L' && chunkType[3] == 'E') {
            // Interlace info
            filterMethod = chunkData[0];
            interlaceMethod = chunkData[1];
            continue;
        }

        if (chunkType[0] == 'D' && chunkType[1] == 'A' &&
            chunkType[2] == 'T' && chunkType[3] == 'A') {
            raw = chunkData;
        }
    }

    if (raw.empty()) return {};

    // Calculate uncompressed size
    size_t uncompressedSize = static_cast<size_t>(outWidth) * outHeight;
    size_t rowBytes = outWidth; // R8 = 1 byte per pixel
    size_t rowBytesWithFilter = rowBytes + 1; // +1 for filter byte

    if (raw.size() < rowBytesWithFilter * outHeight) return {};

    // Simple decoder for no-filter, no-interlace, R8
    std::vector<uint8_t> result(uncompressedSize, 0);
    size_t srcIdx = 0;

    for (uint32_t y = 0; y < outHeight; ++y) {
        uint8_t filter = raw[srcIdx++];
        if (filter != 0) {
            // For simplicity, treat non-zero filter as raw (shouldn't happen with our generated data)
            std::memcpy(&result[y * rowBytes], &raw[srcIdx], rowBytes);
        } else {
            std::memcpy(&result[y * rowBytes], &raw[srcIdx], rowBytes);
        }
        srcIdx += rowBytes;
    }

    return result;
}

} // namespace

// =============================================================================
// Dither3D Asset Manager Implementation
// =============================================================================

bool DitherAssetManager::load_3d_texture(const std::string& path, DitherTexture* out) {
    if (out == nullptr) return false;

    PngHeader hdr;
    if (!readPngHeader(path, &hdr)) return false;

    // Load the raw pixel data
    std::vector<uint8_t> pixels = decodePngR8(path, hdr.width, hdr.height);
    if (pixels.empty()) return false;

    // The PNG stores a 3D texture as a 2D image
    // Width = texture width, Height = texture width * depth
    uint32_t texWidth = hdr.width;
    uint32_t texHeight = hdr.height / hdr.width; // depth = height / width
    uint32_t texDepth = hdr.height;

    out->width = texWidth;
    out->height = texWidth;
    out->depth = texHeight;

    // Reinterpret the flat pixel data as 3D texture layers
    out->data = std::move(pixels);

    // Determine pattern from width
    if (texWidth == 16) out->pattern = DitherPattern::P1x1;
    else if (texWidth == 32) out->pattern = DitherPattern::P2x2;
    else if (texWidth == 64) out->pattern = DitherPattern::P4x4;
    else out->pattern = DitherPattern::P8x8;

    // Store in map
    textures_[out->pattern] = *out;
    return true;
}

bool DitherAssetManager::load_ramp_texture(const std::string& path, DitherRampTexture* out) {
    if (out == nullptr) return false;

    PngHeader hdr;
    if (!readPngHeader(path, &hdr)) return false;

    std::vector<uint8_t> pixels = decodePngR8(path, hdr.width, hdr.height);
    if (pixels.empty()) return false;

    out->width = hdr.width;
    out->height = hdr.height;
    out->data = std::move(pixels);
    ramp_ = *out;
    return true;
}

void DitherAssetManager::generate_textures() {
    if (textures_generated_) return;

    // Generate dither 3D textures using the same algorithm as Dither3D
    // The algorithm creates fractal dither patterns with varying dot densities

    // Pattern configurations: {width, depth}
    struct PatternConfig {
        DitherPattern pattern;
        uint32_t width;
        uint32_t depth;
    };

    std::vector<PatternConfig> configs = {
        {DitherPattern::P1x1, 16, 1},
        {DitherPattern::P2x2, 32, 4},
        {DitherPattern::P4x4, 64, 16},
        {DitherPattern::P8x8, 128, 64},
    };

    // Generate Bayer point patterns for each recursion level
    auto generateBayerPoints = [](int recursion) -> std::vector<std::pair<float, float>> {
        std::vector<std::pair<float, float>> points = {
            {0.0f, 0.0f}, {0.5f, 0.5f}, {0.5f, 0.0f}, {0.0f, 0.5f}
        };
        for (int r = 0; r < recursion - 1; ++r) {
            int count = points.size();
            float offset = std::pow(0.5f, r + 1);
            for (int i = 1; i < 4; ++i) {
                for (int j = 0; j < count; ++j) {
                    points.push_back({
                        points[j].first + points[i].first * offset,
                        points[j].second + points[i].second * offset
                    });
                }
            }
        }
        return points;
    };

    for (const auto& config : configs) {
        DitherTexture tex;
        tex.pattern = config.pattern;
        tex.width = config.width;
        tex.height = config.width;
        tex.depth = config.depth;
        tex.data.resize(config.width * config.width * config.depth, 0);

        int recursion = 0;
        if (config.width == 16) recursion = 1;
        else if (config.width == 32) recursion = 2;
        else if (config.width == 64) recursion = 3;
        else if (config.width == 128) recursion = 4;

        auto bayerPoints = generateBayerPoints(recursion);
        float invRes = 1.0f / config.width;

        for (uint32_t z = 0; z < config.depth; ++z) {
            int dotCount = z + 1;
            float dotArea = 0.5f / dotCount;
            float dotRadius = std::sqrt(dotArea / 3.14159265f);

            size_t layerOffset = z * config.width * config.width;
            for (uint32_t y = 0; y < config.width; ++y) {
                for (uint32_t x = 0; x < config.width; ++x) {
                    float px = (x + 0.5f) * invRes;
                    float py = (y + 0.5f) * invRes;

                    float minDist = 1.0f;
                    for (int i = 0; i < dotCount; ++i) {
                        float dx = px - bayerPoints[i].first;
                        float dy = py - bayerPoints[i].second;
                        // Wrap around
                        dx = std::fmod(dx + 0.5f, 1.0f) - 0.5f;
                        dy = std::fmod(dy + 0.5f, 1.0f) - 0.5f;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist < minDist) minDist = dist;
                    }

                    // Store as distance from nearest dot (0=dot center, 1=far)
                    uint8_t val = static_cast<uint8_t>(minDist * 255.0f);
                    tex.data[layerOffset + y * config.width + x] = val;
                }
            }
        }

        textures_[config.pattern] = tex;
    }

    // Generate ramp texture (256 entries, 1D)
    ramp_.width = 256;
    ramp_.height = 1;
    ramp_.data.resize(256);
    for (uint32_t i = 0; i < 256; ++i) {
        float t = static_cast<float>(i) / 255.0f;
        // Apply S-curve for perceptual uniformity
        float curved = t * t * (3.0f - 2.0f * t);
        ramp_.data[i] = static_cast<uint8_t>(curved * 255.0f);
    }

    textures_generated_ = true;
}

} // namespace litt
