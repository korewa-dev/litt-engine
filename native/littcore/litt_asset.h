// LittAsset - Asset loading system for Litt Engine
// Asset loading module

#pragma once
#include "litt_math.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

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
        
        if (path.substr(path.size() - 4) == ".obj") {
            loadObj(path, *model);
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
        loadTga(path, *texture);
        
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
        
        // Compile shaders
        compileShader(*shader);
        
        shaders[key] = shader;
        return shader;
    }
    
private:
    std::unordered_map<std::string, std::function<std::shared_ptr<void>(const std::string&)>> loaders_;
    
    void loadObj(const std::string& path, Model& model) {
        // Simple OBJ loader
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return;
        
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "v ", 2) == 0) {
                float x, y, z;
                sscanf(line + 2, "%f %f %f", &x, &y, &z);
                model.positions.push_back({x, y, z});
            } else if (strncmp(line, "vn ", 3) == 0) {
                float x, y, z;
                sscanf(line + 3, "%f %f %f", &x, &y, &z);
                model.normals.push_back({x, y, z});
            } else if (strncmp(line, "vt ", 3) == 0) {
                float u, v;
                sscanf(line + 3, "%f %f", &u, &v);
                model.texCoords.push_back({u, v});
            } else if (strncmp(line, "f ", 2) == 0) {
                // Parse face
                int v1, v2, v3;
                sscanf(line + 2, "%d/%d/%d %d/%d/%d %d/%d/%d",
                       &v1, &v1, &v1, &v2, &v2, &v2, &v3, &v3, &v3);
                model.indices.push_back(v1 - 1);
                model.indices.push_back(v2 - 1);
                model.indices.push_back(v3 - 1);
            }
        }
        fclose(f);
    }
    
    void loadTga(const std::string& path, AssetTexture& texture) {
        // Simple TGA loader
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return;
        
        // Read header
        char idLength, colormapType, imageType;
        fread(&idLength, 1, 1, f);
        fread(&colormapType, 1, 1, f);
        fread(&imageType, 1, 1, f);
        
        // Skip to width/height
        fseek(f, 12, SEEK_CUR);
        uint16_t width, height, bpp;
        fread(&width, 2, 1, f);
        fread(&height, 2, 1, f);
        fread(&bpp, 1, 1, f);
        
        texture.width = width;
        texture.height = height;
        texture.channels = bpp / 8;
        
        // Read image data
        size_t imageSize = width * height * texture.channels;
        texture.data.resize(imageSize);
        fread(texture.data.data(), 1, imageSize, f);
        
        fclose(f);
    }
    
    void compileShader(Shader& shader) {
        // Simplified - actual implementation would use OpenGL/Vulkan
    }
};

} // namespace litt
