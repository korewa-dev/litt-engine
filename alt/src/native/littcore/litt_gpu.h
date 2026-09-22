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

enum class GPUBackendSupport {
    TestOnly = 0,
    Supported = 1,
    Unavailable = 3
};

struct GPUBackendCapability {
    const char* name;
    GPUBackendSupport support;
    bool hardware_accelerated;
    bool presentation;
    const char* reason;
};

struct GPURayTracingCapability {
    const char* backend;
    GPUBackendSupport support;
    const char* reason;
};

inline GPURayTracingCapability gpu_ray_tracing_capability(const std::string& backend_name) {
    if (backend_name == "software") {
        return {"software", GPUBackendSupport::Unavailable,
                "software rasterizer does not provide GPU ray tracing"};
    }
    if (backend_name == "vulkan") {
        return {"vulkan", GPUBackendSupport::Unavailable,
                "Vulkan ray tracing is outside the release renderer and not hardware-certified"};
    }
    if (backend_name == "dx12") {
        return {"dx12", GPUBackendSupport::Unavailable,
                "DXR is outside the release renderer and not hardware-certified"};
    }
    if (backend_name == "opengl") {
        return {"opengl", GPUBackendSupport::Unavailable,
                "OpenGL GPU ray tracing is outside the release renderer"};
    }
    if (backend_name == "metal") {
        return {"metal", GPUBackendSupport::Unavailable,
                "Metal ray tracing is outside the release renderer"};
    }
    return {nullptr, GPUBackendSupport::Unavailable, "unknown backend"};
}

inline GPUBackendCapability gpu_backend_capability(const std::string& backend_name) {
    if (backend_name == "null") {
        return {"null", GPUBackendSupport::TestOnly, false, false,
                "headless contract device; does not render or fabricate GPU resources"};
    }
    if (backend_name == "software" || backend_name == "auto") {
        return {"software", GPUBackendSupport::Supported, false,
#ifdef _WIN32
                true,
#else
                false,
#endif
                "bounded CPU rasterizer; headless rendering is release-supported, window presentation is Win32-only"};
    }
    if (backend_name == "vulkan") {
        return {"vulkan", GPUBackendSupport::Unavailable, true, false,
                "generic Vulkan rendering is outside the release renderer and not hardware-certified"};
    }
    if (backend_name == "dx12") {
        return {"dx12", GPUBackendSupport::Unavailable, true, false,
                "generic DX12 rendering is outside the release renderer and not hardware-certified"};
    }
    if (backend_name == "opengl") {
        return {"opengl", GPUBackendSupport::Unavailable, true, false,
                "OpenGL rendering is outside the release renderer"};
    }
    if (backend_name == "metal") {
        return {"metal", GPUBackendSupport::Unavailable, true, false,
                "Metal rendering is outside the release renderer"};
    }
    return {nullptr, GPUBackendSupport::Unavailable, false, false, "unknown backend"};
}

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
            NullBuffer(size_t size, GPUBufferUsage usage) : size_(size), usage_(usage) {}
            void update(const void* data, size_t size) override { (void)data; size_ = size; }
            void* map() override { return nullptr; }
            void unmap() override {}
            size_t get_size() const override { return size_; }
            GPUBufferUsage get_type() const override { return usage_; }
            size_t size_;
            GPUBufferUsage usage_;
        };
        return std::make_unique<NullBuffer>(desc.size, desc.usage);
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

// Factory implemented in litt_gpu.cpp. Hardware backends remain unavailable until
// their full runtime promotion sequence is implemented and physically validated.
std::unique_ptr<IGPUDevice> create_gpu_device(const std::string& backend_name);

} // namespace litt
