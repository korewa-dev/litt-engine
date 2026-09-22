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
#include <sstream>
#include <fstream>
#include <cmath>

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
    static constexpr size_t MAX_MODEL_FILE_BYTES = 32u * 1024u * 1024u;
    static constexpr size_t MAX_MODEL_SOURCE_ELEMENTS = 1000000u;
    static constexpr size_t MAX_MODEL_VERTICES = 3000000u;
    static constexpr size_t MAX_TEXTURE_PIXELS = 16u * 1024u * 1024u;

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
        
        if (path.size() < 4 || path.compare(path.size() - 4, 4, ".obj") != 0 || !loadObj(path, *model)) {
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
        
        if (!loadTga(path, *texture)) return nullptr;

        textures[path] = texture;
        return texture;
    }
    
    std::shared_ptr<Shader> loadShader(const std::string& vertexPath, const std::string& fragmentPath) {
        (void)vertexPath;
        (void)fragmentPath;
        // Source shader compilation is outside the software release contract.
        // Fail explicitly rather than fabricate a compiled shader.
        return nullptr;
    }
    
private:
    std::unordered_map<std::string, std::function<std::shared_ptr<void>(const std::string&)>> loaders_;
    
    bool loadObj(const std::string& path, Model& model) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        const std::streamoff byte_size = file.tellg();
        if (byte_size < 0 || static_cast<uint64_t>(byte_size) > MAX_MODEL_FILE_BYTES) return false;
        file.seekg(0, std::ios::beg);

        struct FaceIndex { int p = 0, t = 0, n = 0; };
        auto resolve = [](int index, size_t count) -> int {
            if (index > 0) return index <= static_cast<int>(count) ? index - 1 : -1;
            if (index < 0) {
                const int resolved = static_cast<int>(count) + index;
                return resolved >= 0 ? resolved : -1;
            }
            return -1;
        };
        auto parse_index = [](const std::string& token, FaceIndex& out) {
            try {
                const size_t first = token.find('/');
                if (first == std::string::npos) {
                    out.p = std::stoi(token);
                    return true;
                }
                const std::string p = token.substr(0, first);
                if (p.empty()) return false;
                out.p = std::stoi(p);
                const size_t second = token.find('/', first + 1);
                const std::string t = token.substr(first + 1,
                    second == std::string::npos ? std::string::npos : second - first - 1);
                if (!t.empty()) out.t = std::stoi(t);
                if (second != std::string::npos) {
                    const std::string n = token.substr(second + 1);
                    if (!n.empty()) out.n = std::stoi(n);
                }
                return true;
            } catch (...) {
                return false;
            }
        };

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string kind;
            if (!(stream >> kind) || kind[0] == '#') continue;
            if (kind == "v") {
                float x, y, z;
                if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) ||
                    !std::isfinite(z) || model.positions.size() >= MAX_MODEL_SOURCE_ELEMENTS) {
                    return false;
                }
                model.positions.push_back({x, y, z});
            } else if (kind == "vn") {
                float x, y, z;
                if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) ||
                    !std::isfinite(z) || model.normals.size() >= MAX_MODEL_SOURCE_ELEMENTS) {
                    return false;
                }
                model.normals.push_back({x, y, z});
            } else if (kind == "vt") {
                float u, v;
                if (!(stream >> u >> v) || !std::isfinite(u) || !std::isfinite(v) ||
                    model.texCoords.size() >= MAX_MODEL_SOURCE_ELEMENTS) {
                    return false;
                }
                model.texCoords.push_back({u, v});
            } else if (kind == "f") {
                std::vector<FaceIndex> face;
                std::string token;
                while (stream >> token) {
                    FaceIndex index;
                    if (!parse_index(token, index)) { face.clear(); break; }
                    face.push_back(index);
                }
                if (face.size() < 3) continue;
                for (size_t i = 1; i + 1 < face.size(); ++i) {
                    const FaceIndex tri[3] = {face[0], face[i], face[i + 1]};
                    for (const FaceIndex& source : tri) {
                        const int p = resolve(source.p, model.positions.size());
                        if (p < 0) { model.vertices.clear(); model.indices.clear(); return false; }
                        Vertex vertex{};
                        vertex.position = model.positions[static_cast<size_t>(p)];
                        const int n = resolve(source.n, model.normals.size());
                        if (n >= 0) vertex.normal = model.normals[static_cast<size_t>(n)];
                        const int t = resolve(source.t, model.texCoords.size());
                        if (t >= 0) vertex.texCoord = model.texCoords[static_cast<size_t>(t)];
                        vertex.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                        if (model.vertices.size() >= MAX_MODEL_VERTICES) return false;
                        model.vertices.push_back(vertex);
                        model.indices.push_back(static_cast<uint32_t>(model.vertices.size() - 1));
                    }
                }
            }
        }
        return !model.positions.empty() && !model.vertices.empty() && !model.indices.empty();
    }

    bool loadTga(const std::string& path, AssetTexture& texture) {
        // Simple TGA loader
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        
        // Read header
        uint8_t idLength = 0, colormapType = 0, imageType = 0;
        if (fread(&idLength, 1, 1, f) != 1 ||
            fread(&colormapType, 1, 1, f) != 1 ||
            fread(&imageType, 1, 1, f) != 1) {
            fclose(f); return false;
        }
        
        // TGA width starts at byte 12. We already consumed 3 bytes.
        if (fseek(f, 9, SEEK_CUR) != 0) { fclose(f); return false; }
        uint16_t width = 0, height = 0;
        uint8_t bpp = 0, descriptor = 0;
        if (fread(&width, 2, 1, f) != 1 || fread(&height, 2, 1, f) != 1 ||
            fread(&bpp, 1, 1, f) != 1 || fread(&descriptor, 1, 1, f) != 1) {
            fclose(f); return false;
        }
        if (colormapType != 0 || (imageType != 2 && imageType != 3) ||
            width == 0 || height == 0 || (bpp != 8 && bpp != 24 && bpp != 32)) {
            fclose(f); return false;
        }
        if (idLength && fseek(f, static_cast<long>(idLength), SEEK_CUR) != 0) {
            fclose(f); return false;
        }
        
        texture.width = width;
        texture.height = height;
        texture.channels = bpp / 8;
        
        // Read image data
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixelCount > MAX_TEXTURE_PIXELS || texture.channels == 0 ||
            pixelCount > std::numeric_limits<size_t>::max() / texture.channels) {
            fclose(f); return false;
        }
        const size_t imageSize = pixelCount * texture.channels;
        texture.data.resize(imageSize);
        if (fread(texture.data.data(), 1, imageSize, f) != imageSize) {
            texture.data.clear(); fclose(f); return false;
        }

        // TGA true-color data is BGR(A); normalize to RGB(A).
        if (texture.channels >= 3) {
            for (size_t i = 0; i < imageSize; i += texture.channels) {
                std::swap(texture.data[i], texture.data[i + 2]);
            }
        }
        // Normalize origin to top-left. Bit 5 selects top vs bottom origin;
        // bit 4 selects left vs right origin.
        const size_t rowBytes = static_cast<size_t>(width) * texture.channels;
        if ((descriptor & 0x20u) == 0u) {
            std::vector<uint8_t> row(rowBytes);
            for (uint32_t y = 0; y < height / 2; ++y) {
                uint8_t* top = texture.data.data() + static_cast<size_t>(y) * rowBytes;
                uint8_t* bottom = texture.data.data() +
                    static_cast<size_t>(height - 1 - y) * rowBytes;
                std::memcpy(row.data(), top, rowBytes);
                std::memcpy(top, bottom, rowBytes);
                std::memcpy(bottom, row.data(), rowBytes);
            }
        }
        if ((descriptor & 0x10u) != 0u) {
            for (uint32_t y = 0; y < height; ++y) {
                uint8_t* row = texture.data.data() + static_cast<size_t>(y) * rowBytes;
                for (uint32_t x = 0; x < width / 2; ++x) {
                    uint8_t* left = row + static_cast<size_t>(x) * texture.channels;
                    uint8_t* right = row + static_cast<size_t>(width - 1 - x) * texture.channels;
                    for (uint32_t channel = 0; channel < texture.channels; ++channel)
                        std::swap(left[channel], right[channel]);
                }
            }
        }
        fclose(f);
        return true;
    }
};

} // namespace litt
