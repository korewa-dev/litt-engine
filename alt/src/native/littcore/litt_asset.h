// LittAsset - Asset loading system for Litt Engine
// Asset loading module

#pragma once
#include "litt_math.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <typeinfo>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace litt {

// =============================================================================
// Vertex
// =============================================================================
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
    Vec4 color;
};

// =============================================================================
// Model
// =============================================================================
struct Model {
    std::string path;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;
    
    Aabb bounds;
    
    void computeBounds() {
        if (positions.empty()) return;
        
        bounds.min = positions[0];
        bounds.max = positions[0];
        
        for (const auto& v : positions) {
            bounds.min.x = std::min(bounds.min.x, v.x);
            bounds.min.y = std::min(bounds.min.y, v.y);
            bounds.min.z = std::min(bounds.min.z, v.z);
            bounds.max.x = std::max(bounds.max.x, v.x);
            bounds.max.y = std::max(bounds.max.y, v.y);
            bounds.max.z = std::max(bounds.max.z, v.z);
        }
    }
};

// =============================================================================
// Asset Texture (raw pixel data - separate from GPU Texture class in litt_texture.h)
// =============================================================================
struct AssetTexture {
    std::string path;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 4;
    std::vector<uint8_t> data;
};

// =============================================================================
// Shader
// =============================================================================
struct Shader {
    std::string vertexPath;
    std::string fragmentPath;
    std::string geometryPath;
    std::string computePath;
    
    std::unordered_map<std::string, int> uniformLocations;
    std::unordered_map<std::string, int> attributeLocations;
};

// =============================================================================
// Asset Material (raw material data - separate from GPU PBRMaterial)
// =============================================================================
struct AssetMaterial {
    std::string name;
    std::shared_ptr<Shader> shader;
    std::unordered_map<std::string, float> uniforms;
    std::unordered_map<std::string, std::shared_ptr<AssetTexture>> textures;
    
    // PBR values
    Vec3 albedo = Vec3(0.8f, 0.8f, 0.8f);
    float roughness = 0.5f;
    float metalness = 0.0f;
    float occlusion = 1.0f;
    float emissive = 0.0f;
};

// =============================================================================
// Asset Manager
// =============================================================================
class AssetManager {
public:
    std::unordered_map<std::string, std::shared_ptr<Model>> models;
    std::unordered_map<std::string, std::shared_ptr<AssetTexture>> textures;
    std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    std::unordered_map<std::string, std::shared_ptr<AssetMaterial>> materials;
    
    template<typename T>
    std::shared_ptr<T> load(const std::string& path) {
        auto it = loaders_.find(typeid(T).name());
        if (it == loaders_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second(path));
    }
    
    template<typename T>
    void registerLoader(const std::function<std::shared_ptr<T>(const std::string&)>& loader) {
        loaders_[typeid(T).name()] = loader;
    }
    
    std::shared_ptr<Model> loadModel(const std::string& path) {
        auto it = models.find(path);
        if (it != models.end()) return it->second;

        if (!hasExtension(path, ".obj")) return nullptr;
        auto model = std::make_shared<Model>();
        model->path = path;
        if (!loadObj(path, *model)) return nullptr;

        model->computeBounds();
        models.emplace(path, model);
        return model;
    }

    std::shared_ptr<AssetTexture> loadTexture(const std::string& path) {
        auto it = textures.find(path);
        if (it != textures.end()) return it->second;

        if (!hasExtension(path, ".tga")) return nullptr;
        auto texture = std::make_shared<AssetTexture>();
        texture->path = path;
        if (!loadTga(path, *texture)) return nullptr;

        textures.emplace(path, texture);
        return texture;
    }

    std::shared_ptr<Shader> loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
        (void)vertexPath;
        (void)fragmentPath;
        // No shader compiler backend is wired into AssetManager. Returning
        // nullptr is deliberate: an unvalidated placeholder must never be
        // cached or reported as a compiled shader.
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::function<std::shared_ptr<void>(const std::string&)>> loaders_;
    
    static constexpr size_t kMaxAssetBytes = 256u * 1024u * 1024u;
    static constexpr size_t kMaxVertices = 4u * 1024u * 1024u;
    static constexpr size_t kMaxIndices = 12u * 1024u * 1024u;
    static constexpr uint32_t kMaxTextureDimension = 16384u;

    static bool hasExtension(const std::string& path, const char* ext) {
        const size_t n = std::strlen(ext);
        if (path.size() < n) return false;
        return path.compare(path.size() - n, n, ext) == 0;
    }

    static bool parseObjIndex(const char* token, int count, int& out) {
        if (!token || !*token) return false;
        char* end = nullptr;
        long v = std::strtol(token, &end, 10);
        if (end == token || v == 0) return false;
        long resolved = v > 0 ? v - 1 : static_cast<long>(count) + v;
        if (resolved < 0 || resolved >= count) return false;
        out = static_cast<int>(resolved);
        return true;
    }

    bool loadObj(const std::string& path, Model& model) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;

        if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
        const long fileSize = std::ftell(f);
        if (fileSize <= 0 || static_cast<size_t>(fileSize) > kMaxAssetBytes) {
            std::fclose(f);
            return false;
        }
        std::rewind(f);

        char line[4096];
        while (std::fgets(line, sizeof(line), f)) {
            if (!std::strchr(line, '\n') && !std::feof(f)) {
                std::fclose(f);
                return false; // reject overlong/truncated logical lines
            }

            char* p = line;
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;

            if (std::strncmp(p, "v ", 2) == 0) {
                float x, y, z;
                if (std::sscanf(p + 2, "%f %f %f", &x, &y, &z) != 3 ||
                    model.positions.size() >= kMaxVertices) {
                    std::fclose(f);
                    return false;
                }
                model.positions.push_back({x, y, z});
            } else if (std::strncmp(p, "vn ", 3) == 0) {
                float x, y, z;
                if (std::sscanf(p + 3, "%f %f %f", &x, &y, &z) != 3 ||
                    model.normals.size() >= kMaxVertices) {
                    std::fclose(f);
                    return false;
                }
                model.normals.push_back({x, y, z});
            } else if (std::strncmp(p, "vt ", 3) == 0) {
                float u, v;
                if (std::sscanf(p + 3, "%f %f", &u, &v) != 2 ||
                    model.texCoords.size() >= kMaxVertices) {
                    std::fclose(f);
                    return false;
                }
                model.texCoords.push_back({u, v});
            } else if (std::strncmp(p, "f ", 2) == 0) {
                std::vector<uint32_t> face;
                char* save = nullptr;
                for (char* tok = strtok_r(p + 2, " \t\r\n", &save);
                     tok; tok = strtok_r(nullptr, " \t\r\n", &save)) {
                    char* slash = std::strchr(tok, '/');
                    if (slash) *slash = '\0';
                    int vi = 0;
                    if (!parseObjIndex(tok, static_cast<int>(model.positions.size()), vi)) {
                        std::fclose(f);
                        return false;
                    }
                    face.push_back(static_cast<uint32_t>(vi));
                    if (face.size() > 256u) { std::fclose(f); return false; }
                }
                if (face.size() < 3u) { std::fclose(f); return false; }
                const size_t added = (face.size() - 2u) * 3u;
                if (model.indices.size() > kMaxIndices - added) {
                    std::fclose(f);
                    return false;
                }
                for (size_t i = 2; i < face.size(); ++i) {
                    model.indices.push_back(face[0]);
                    model.indices.push_back(face[i - 1]);
                    model.indices.push_back(face[i]);
                }
            }
        }

        const bool ok = !std::ferror(f) && !model.positions.empty() && !model.indices.empty();
        std::fclose(f);
        return ok;
    }

    bool loadTga(const std::string& path, AssetTexture& texture) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;

        uint8_t h[18];
        if (std::fread(h, 1, sizeof(h), f) != sizeof(h)) {
            std::fclose(f);
            return false;
        }

        const uint8_t idLength = h[0];
        const uint8_t colorMapType = h[1];
        const uint8_t imageType = h[2];
        const uint16_t width = static_cast<uint16_t>(h[12] | (uint16_t(h[13]) << 8));
        const uint16_t height = static_cast<uint16_t>(h[14] | (uint16_t(h[15]) << 8));
        const uint8_t bpp = h[16];
        const uint8_t descriptor = h[17];

        // Explicit supported subset: uncompressed true-color, no color map,
        // 24/32-bit pixels. Right-to-left images are rejected.
        if (colorMapType != 0 || imageType != 2 || width == 0 || height == 0 ||
            width > kMaxTextureDimension || height > kMaxTextureDimension ||
            (bpp != 24 && bpp != 32) || (descriptor & 0x10u)) {
            std::fclose(f);
            return false;
        }
        if (std::fseek(f, idLength, SEEK_CUR) != 0) {
            std::fclose(f);
            return false;
        }

        const size_t channels = bpp / 8u;
        const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixels > kMaxAssetBytes / channels) {
            std::fclose(f);
            return false;
        }

        std::vector<uint8_t> src(pixels * channels);
        if (std::fread(src.data(), 1, src.size(), f) != src.size()) {
            std::fclose(f);
            return false;
        }
        std::fclose(f);

        texture.width = width;
        texture.height = height;
        texture.channels = static_cast<uint32_t>(channels);
        texture.data.resize(src.size());

        const bool topOrigin = (descriptor & 0x20u) != 0;
        for (uint32_t y = 0; y < height; ++y) {
            const uint32_t sy = topOrigin ? y : (height - 1u - y);
            for (uint32_t x = 0; x < width; ++x) {
                const size_t s = (static_cast<size_t>(sy) * width + x) * channels;
                const size_t d = (static_cast<size_t>(y) * width + x) * channels;
                texture.data[d + 0] = src[s + 2]; // BGR(A) -> RGB(A)
                texture.data[d + 1] = src[s + 1];
                texture.data[d + 2] = src[s + 0];
                if (channels == 4u) texture.data[d + 3] = src[s + 3];
            }
        }
        return true;
    }
};

} // namespace litt
