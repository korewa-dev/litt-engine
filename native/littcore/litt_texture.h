// Phase 3: Rendering Pipeline - Texture System

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace litt {

// Texture format
enum class TextureFormat {
    RGBA8,
    RGBA16F,
    RGBA32F,
    RGB8,
    RGB16F,
    RGB32F,
    RG8,
    R8,
    DEPTH24_STENCIL8,
    DEPTH32,
    BC1,
    BC3,
    BC5,
    BC7
};

// Texture wrap mode
enum class TextureWrap {
    REPEAT,
    CLAMP_TO_EDGE,
    MIRRORED_REPEAT,
    CLAMP_TO_BORDER
};

// Texture filter
enum class TextureFilter {
    NEAREST,
    LINEAR,
    NEAREST_MIPMAP_NEAREST,
    LINEAR_MIPMAP_NEAREST,
    NEAREST_MIPMAP_LINEAR,
    LINEAR_MIPMAP_LINEAR
};

// Texture description
struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    TextureWrap wrap_u = TextureWrap::REPEAT;
    TextureWrap wrap_v = TextureWrap::REPEAT;
    TextureFilter min_filter = TextureFilter::LINEAR_MIPMAP_LINEAR;
    TextureFilter mag_filter = TextureFilter::LINEAR;
    bool generate_mipmaps = true;
    bool srgb = false;
};

// Texture base class
class Texture {
public:
    virtual ~Texture() = default;
    
    // Bind texture
    void bind(uint32_t slot = 0) const;
    void unbind() const;
    
    // Get texture ID
    uint32_t get_id() const { return texture_id_; }
    
    // Get dimensions
    uint32_t get_width() const { return desc_.width; }
    uint32_t get_height() const { return desc_.height; }
    
    // Get format
    TextureFormat get_format() const { return desc_.desc_.format; }
    
    // Set data
    void set_data(const void* data, size_t size);
    
    // Generate mipmaps
    void generate_mipmaps();

protected:
    Texture(const TextureDesc& desc);
    
    TextureDesc desc_;
    uint32_t texture_id_ = 0;
};

// 2D Texture
class Texture2D : public Texture {
public:
    Texture2D(const TextureDesc& desc);
    ~Texture2D() = default;
    
    // Load from file
    static std::unique_ptr<Texture2D> load_from_file(const std::string& path);
    
    // Create from data
    static std::unique_ptr<Texture2D> create(uint32_t width, uint32_t height, 
                                             TextureFormat format,
                                             const void* data = nullptr);
};

// Cubemap texture
class Cubemap : public Texture {
public:
    Cubemap(const TextureDesc& desc);
    ~Cubemap() = default;
    
    // Load from file (single image or 6 faces)
    static std::unique_ptr<Cubemap> load_from_file(const std::string& path);
    
    // Create from 6 faces
    static std::unique_ptr<Cubemap> create(const std::vector<std::string>& face_paths);
};

// Texture atlas
class TextureAtlas {
public:
    TextureAtlas(uint32_t width, uint32_t height);
    ~TextureAtlas();
    
    // Add sub-texture
    bool add_subtexture(const std::string& name, uint32_t x, uint32_t y, 
                        uint32_t width, uint32_t height);
    
    // Get UV coordinates
    Vec4 get_uv_coords(const std::string& name) const;
    
    // Get texture ID
    uint32_t get_id() const { return texture_id_; }

private:
    uint32_t texture_id_;
    uint32_t width_;
    uint32_t height_;
    std::unordered_map<std::string, Vec4> subtextures_;
};

// Texture manager
class TextureManager {
public:
    static TextureManager& get_instance() {
        static TextureManager instance;
        return instance;
    }
    
    // Load texture
    Texture* load_texture(const std::string& name, const std::string& path);
    
    // Create texture
    Texture* create_texture(const std::string& name, const TextureDesc& desc);
    
    // Get texture
    Texture* get_texture(const std::string& name);
    
    // Remove texture
    void remove_texture(const std::string& name);
    
    // Clear all textures
    void clear();

private:
    TextureManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
};

} // namespace litt
