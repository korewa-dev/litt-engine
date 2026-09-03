// LittGPU - GPU abstraction layer for Litt Engine
// Platform-independent GPU interface with Vulkan/DirectX backends
// TextureFormat, TextureDesc from litt_texture.h; RenderTarget from litt_render_pass.h

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>

#include "litt_texture.h"
#include "litt_render_pass.h"

namespace litt {

// =============================================================================
// GPU Abstraction Types
// =============================================================================

enum class GPUBufferUsage {
    VERTEX,
    INDEX,
    CONSTANT,
    STORAGE,
    UNIFORM,
    INDIRECT
};

enum class GPUBufferFlags {
    NONE = 0,
    CPU_VISIBLE = 1 << 0,
    CPU_WRITE = 1 << 1,
    GPU_READ = 1 << 2,
    GPU_WRITE = 1 << 3,
    SHADER_RESOURCE = 1 << 4,
    UNORDERED_ACCESS = 1 << 5
};

enum class TextureUsage {
    SHADER_READ,
    RENDER_TARGET,
    DEPTH_STENCIL,
    COPY_DST
};

struct BufferDesc {
    size_t size = 0;
    GPUBufferUsage usage = GPUBufferUsage::VERTEX;
    uint32_t flags = 0;
    const void* data = nullptr;
    bool dynamic = false;
};

// =============================================================================
// GPU Resource Interfaces
// =============================================================================

class GPUBuffer {
public:
    virtual ~GPUBuffer() = default;
    virtual void update(const void* data, size_t size) = 0;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual size_t get_size() const = 0;
    virtual GPUBufferUsage get_type() const = 0;
};

class GPUTexture {
public:
    virtual ~GPUTexture() = default;
    virtual void update(const void* data, uint32_t width, uint32_t height) = 0;
    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
    virtual TextureFormat get_format() const = 0;
};

class GPUShader {
public:
    virtual ~GPUShader() = default;
    virtual void bind() = 0;
    virtual void set_uniform(const std::string& name, const void* data, size_t size) = 0;
};

// =============================================================================
// GPU Device Interface
// =============================================================================

class IGPUDevice {
public:
    virtual ~IGPUDevice() = default;

    // Lifecycle
    virtual bool initialize(const std::string& adapter_name = "") = 0;
    virtual void shutdown() = 0;
    virtual void present() = 0;

    // Resource creation
    virtual std::unique_ptr<GPUBuffer> create_buffer(const BufferDesc& desc) = 0;
    virtual void update_buffer(GPUBuffer* buffer, const void* data, size_t size) = 0;
    virtual void destroy_buffer(GPUBuffer* buffer) = 0;

    virtual std::unique_ptr<GPUTexture> create_texture(const TextureDesc& desc) = 0;
    virtual void destroy_texture(GPUTexture* texture) = 0;

    // Shader
    virtual std::unique_ptr<GPUShader> create_shader(const std::string& vertex_src,
                                                      const std::string& fragment_src) = 0;
    virtual std::unique_ptr<GPUShader> create_shader_from_file(const std::string& path) = 0;

    // Render target
    virtual std::shared_ptr<RenderTarget> create_render_target(uint32_t width, uint32_t height) = 0;

    // Stats
    virtual size_t get_vram_usage() const = 0;
};

// Null GPU device (headless / testing)
class NullGPUDevice : public IGPUDevice {
public:
    bool initialize(const std::string& adapter_name = "") override { (void)adapter_name; return true; }
    void shutdown() override {}
    void present() override {}
    std::unique_ptr<GPUBuffer> create_buffer(const BufferDesc& desc) override {
        struct NullBuffer : GPUBuffer {
            size_t sz = 0;
            void update(const void* data, size_t size) override { sz = size; (void)data; }
            void* map() override { return nullptr; }
            void unmap() override {}
            size_t get_size() const override { return sz; }
            GPUBufferUsage get_type() const override { return GPUBufferUsage::VERTEX; }
        };
        return std::make_unique<NullBuffer>();
    }
    void update_buffer(GPUBuffer* buffer, const void* data, size_t size) override {
        if (buffer) buffer->update(data, size);
    }
    void destroy_buffer(GPUBuffer* buffer) override { (void)buffer; }
    std::unique_ptr<GPUTexture> create_texture(const TextureDesc& desc) override {
        struct NullTexture : GPUTexture {
            uint32_t w = 0, h = 0;
            TextureFormat fmt;
            NullTexture(uint32_t w, uint32_t h, TextureFormat f) : w(w), h(h), fmt(f) {}
            void update(const void* data, uint32_t width, uint32_t height) override { w = width; h = height; (void)data; }
            uint32_t get_width() const override { return w; }
            uint32_t get_height() const override { return h; }
            TextureFormat get_format() const override { return fmt; }
        };
        return std::make_unique<NullTexture>(desc.width, desc.height, desc.format);
    }
    void destroy_texture(GPUTexture* texture) override { (void)texture; }
    std::unique_ptr<GPUShader> create_shader(const std::string& vs, const std::string& fs) override {
        struct NullShader : GPUShader {
            void bind() override {}
            void set_uniform(const std::string& name, const void* data, size_t size) override { (void)name; (void)data; (void)size; }
        };
        (void)vs; (void)fs;
        return std::make_unique<NullShader>();
    }
    std::unique_ptr<GPUShader> create_shader_from_file(const std::string& path) override {
        struct NullShader : GPUShader {
            void bind() override {}
            void set_uniform(const std::string& name, const void* data, size_t size) override { (void)name; (void)data; (void)size; }
        };
        (void)path;
        return std::make_unique<NullShader>();
    }
    std::shared_ptr<RenderTarget> create_render_target(uint32_t w, uint32_t h) override {
        return nullptr; // RenderTarget requires real GPU; null is fine for headless
    }
    size_t get_vram_usage() const override { return 0; }
};

} // namespace litt
