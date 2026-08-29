// LittGPU - GPU abstraction layer for Litt Engine
// Platform-independent GPU interface with Vulkan/DirectX backends

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>

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

enum class TextureFormat {
    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA16_FLOAT,
    RGBA32_FLOAT,
    D32_FLOAT,
    D16_UNORM,
    BC1_UNORM,
    BC3_UNORM
};

enum class TextureUsage {
    SHADER_READ,
    RENDER_TARGET,
    DEPTH_STENCIL,
    COPY_DST
};

struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_levels = 1;
    uint32_t array_size = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    TextureUsage usage = TextureUsage::SHADER_READ;
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
    virtual void bind() const = 0;
    virtual void set_uniform(const std::string& name, const void* data, size_t size) = 0;
};

class RenderTarget {
public:
    virtual ~RenderTarget() = default;
    virtual void bind() const = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
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
    virtual void update_texture(GPUTexture* texture, const void* data, uint32_t width, uint32_t height) = 0;
    virtual void destroy_texture(GPUTexture* texture) = 0;

    virtual std::unique_ptr<GPUShader> create_shader(const std::string& source, const std::string& entry_point = "main") = 0;
    virtual void destroy_shader(GPUShader* shader) = 0;

    virtual std::unique_ptr<RenderTarget> create_render_target(const TextureDesc& desc) = 0;
    virtual void destroy_render_target(RenderTarget* target) = 0;

    // Drawing
    virtual void drawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0) = 0;
    virtual void drawArrays(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;

    // State management
    virtual void set_pipeline(const std::string& pipeline_name) = 0;
    virtual void set_vertex_buffer(GPUBuffer* buffer, uint32_t offset = 0, uint32_t stride = 0) = 0;
    virtual void set_index_buffer(GPUBuffer* buffer) = 0;

    // Queries
    virtual std::string get_adapter_name() const = 0;
    virtual bool is_ray_tracing_supported() const = 0;
    virtual uint32_t get_max_texture_size() const = 0;
};

// =============================================================================
// GPU Factory
// =============================================================================

inline std::unique_ptr<IGPUDevice> create_gpu_device(const std::string& backend_name = "auto") {
    // Note: This is a stub implementation.
    // In a real engine, this would create platform-specific GPU devices.
    // For now, return nullptr to indicate no GPU backend is available.
    throw std::runtime_error("GPU backend not implemented. Implement VulkanDevice or D3D12Device classes.");
}

// =============================================================================
// Null GPU Device (for testing and headless mode)
// =============================================================================

class NullGPUDevice : public IGPUDevice {
public:
    bool initialize(const std::string& adapter_name = "") override {
        adapter_name_ = adapter_name.empty() ? "Null GPU" : adapter_name;
        return true;
    }

    void shutdown() override {}
    void present() override {}

    std::unique_ptr<GPUBuffer> create_buffer(const BufferDesc& desc) override {
        return nullptr;
    }

    void update_buffer(GPUBuffer* buffer, const void* data, size_t size) override {}
    void destroy_buffer(GPUBuffer* buffer) override {}

    std::unique_ptr<GPUTexture> create_texture(const TextureDesc& desc) override {
        return nullptr;
    }

    void update_texture(GPUTexture* texture, const void* data, uint32_t width, uint32_t height) override {}
    void destroy_texture(GPUTexture* texture) override {}

    std::unique_ptr<GPUShader> create_shader(const std::string& source, const std::string& entry_point = "main") override {
        return nullptr;
    }

    void destroy_shader(GPUShader* shader) override {}

    std::unique_ptr<RenderTarget> create_render_target(const TextureDesc& desc) override {
        return nullptr;
    }

    void destroy_render_target(RenderTarget* target) override {}

    void drawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index) override {}
    void drawArrays(uint32_t vertex_count, uint32_t first_vertex) override {}

    void set_pipeline(const std::string& pipeline_name) override {}
    void set_vertex_buffer(GPUBuffer* buffer, uint32_t offset, uint32_t stride) override {}
    void set_index_buffer(GPUBuffer* buffer) override {}

    std::string get_adapter_name() const override { return adapter_name_; }
    bool is_ray_tracing_supported() const override { return false; }
    uint32_t get_max_texture_size() const override { return 8192; }

private:
    std::string adapter_name_;
};

// Factory function for null device
inline std::unique_ptr<IGPUDevice> create_null_gpu_device() {
    return std::make_unique<NullGPUDevice>();
}

} // namespace litt
