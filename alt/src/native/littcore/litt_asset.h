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
#include <limits>

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
    Vec3 albedo;
    float roughness;
    float metalness;
    float occlusion;
    float emissive;
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
        
        // Load OBJ
        auto model = std::make_shared<Model>();
        model->path = path;
        
        if (path.size() < 4 || path.substr(path.size() - 4) != ".obj" || !loadObj(path, *model)) {
            return nullptr;
        }
        model->computeBounds();
        models[path] = model;
        return model;
    }
    
    std::shared_ptr<AssetTexture> loadTexture(const std::string& path) {
        auto it = textures.find(path);
        if (it != textures.end()) return it->second;
        
        // Load image
        auto texture = std::make_shared<AssetTexture>();
        texture->path = path;
        
        // TGA loader (simple)
        if (!loadTga(path, *texture)) return nullptr;
        textures[path] = texture;
        return texture;
    }
    
    std::shared_ptr<Shader> loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
        auto key = vertexPath + ":" + fragmentPath;
        auto it = shaders.find(key);
        if (it != shaders.end()) return it->second;
        
        auto shader = std::make_shared<Shader>();
        shader->vertexPath = vertexPath;
        shader->fragmentPath = fragmentPath;
        
        // Generic GPU shader compilation is not release-supported yet.
        // Never cache or return a fake-success shader object.
        if (!compileShader(*shader)) return nullptr;
        shaders[key] = shader;
        return shader;
    }
    
private:
    std::unordered_map<std::string, std::function<std::shared_ptr<void>(const std::string&)>> loaders_;
    
    bool loadObj(const std::string& path, Model& model) {
        // Simple OBJ loader
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;
        bool valid = true;
        
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "v ", 2) == 0) {
                float x, y, z;
                if (sscanf(line + 2, "%f %f %f", &x, &y, &z) != 3) { valid = false; break; }
                model.positions.push_back({x, y, z});
            } else if (strncmp(line, "vn ", 3) == 0) {
                float x, y, z;
                if (sscanf(line + 3, "%f %f %f", &x, &y, &z) != 3) { valid = false; break; }
                model.normals.push_back({x, y, z});
            } else if (strncmp(line, "vt ", 3) == 0) {
                float u, v;
                if (sscanf(line + 3, "%f %f", &u, &v) != 2) { valid = false; break; }
                model.texCoords.push_back({u, v});
            } else if (strncmp(line, "f ", 2) == 0) {
                // Parse face
                int v1 = 0, v2 = 0, v3 = 0;
                if (sscanf(line + 2, "%d %d %d", &v1, &v2, &v3) != 3 ||
                    v1 <= 0 || v2 <= 0 || v3 <= 0 ||
                    static_cast<size_t>(v1) > model.positions.size() ||
                    static_cast<size_t>(v2) > model.positions.size() ||
                    static_cast<size_t>(v3) > model.positions.size()) { valid = false; break; }
                model.indices.push_back(static_cast<uint32_t>(v1 - 1));
                model.indices.push_back(static_cast<uint32_t>(v2 - 1));
                model.indices.push_back(static_cast<uint32_t>(v3 - 1));
            }
        }
        fclose(f);
        return valid && !model.positions.empty() && !model.indices.empty();
    }
    
    bool loadTga(const std::string& path, AssetTexture& texture) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;

        unsigned char header[18] = {};
        if (fread(header, 1, sizeof(header), f) != sizeof(header)) { fclose(f); return false; }
        const uint8_t idLength = header[0];
        const uint8_t colormapType = header[1];
        const uint8_t imageType = header[2];
        const uint16_t width = static_cast<uint16_t>(header[12] | (header[13] << 8));
        const uint16_t height = static_cast<uint16_t>(header[14] | (header[15] << 8));
        const uint8_t bpp = header[16];
        if (colormapType != 0 || imageType != 2 || width == 0 || height == 0 ||
            (bpp != 24 && bpp != 32)) { fclose(f); return false; }

        const size_t channels = bpp / 8u;
        if (static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / height ||
            static_cast<size_t>(width) * height > std::numeric_limits<size_t>::max() / channels) {
            fclose(f); return false;
        }
        const size_t imageSize = static_cast<size_t>(width) * height * channels;
        constexpr size_t kMaxImageBytes = 256u * 1024u * 1024u;
        if (imageSize == 0 || imageSize > kMaxImageBytes ||
            fseek(f, static_cast<long>(idLength), SEEK_CUR) != 0) { fclose(f); return false; }

        std::vector<uint8_t> data(imageSize);
        const bool ok = fread(data.data(), 1, imageSize, f) == imageSize;
        fclose(f);
        if (!ok) return false;
        texture.width = width;
        texture.height = height;
        texture.channels = static_cast<uint32_t>(channels);
        texture.data = std::move(data);
        return true;
    }

    bool compileShader(Shader&) {
        return false;
    }
};

} // namespace litt
