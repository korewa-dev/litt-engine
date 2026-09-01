// Phase 3: Rendering Pipeline - Render Pass System

#pragma once

#include "litt_math.h"
#include <vector>
#include <memory>
#include <string>

namespace litt {

// Render target
class RenderTarget {
public:
    RenderTarget(uint32_t width, uint32_t height);
    ~RenderTarget();
    
    // Bind/unbind render target
    void bind() const;
    void unbind() const;
    
    // Resize
    void resize(uint32_t width, uint32_t height);
    
    // Clear
    void clear(const Vec4& color = Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    
    // Get texture ID
    uint32_t get_texture_id() const { return color_texture_; }
    
    // Get depth ID
    uint32_t get_depth_id() const { return depth_texture_; }
    
    // Get dimensions
    uint32_t get_width() const { return width_; }
    uint32_t get_height() const { return height_; }
    
    // Set MSAA samples
    void set_msaa_samples(uint32_t samples) { msaa_samples_ = samples; }

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t color_texture_;
    uint32_t depth_texture_;
    uint32_t framebuffer_;
    uint32_t msaa_samples_ = 0;
};

// Render pass types
enum class RenderPassType {
    SHADOW,
    GEOMETRY,
    LIGHTING,
    POST_PROCESS,
    UI,
    COMPUTE
};

// Render pass base class
class RenderPass {
public:
    RenderPass(RenderPassType type) : type_(type) {}
    virtual ~RenderPass() = default;
    
    // Execute pass
    virtual void execute() = 0;
    
    // Get pass type
    RenderPassType get_type() const { return type_; }
    
    // Get pass name
    const std::string& get_name() const { return name_; }
    
    // Enable/disable
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

protected:
    RenderPassType type_;
    std::string name_;
    bool enabled_ = true;
};

// Shadow pass
class ShadowPass : public RenderPass {
public:
    ShadowPass();
    
    void execute() override;
    
    // Set shadow map
    void set_shadow_map(std::shared_ptr<RenderTarget> shadow_map) { shadow_map_ = shadow_map; }
    
    // Get shadow map
    RenderTarget* get_shadow_map() const { return shadow_map_.get(); }

private:
    std::shared_ptr<RenderTarget> shadow_map_;
    Mat4 light_view_proj_;
};

// Geometry pass
class GeometryPass : public RenderPass {
public:
    GeometryPass();
    
    void execute() override;
    
    // Set render target
    void set_render_target(std::shared_ptr<RenderTarget> target) { render_target_ = target; }
    
    // Get render target
    RenderTarget* get_render_target() const { return render_target_.get(); }

private:
    std::shared_ptr<RenderTarget> render_target_;
};

// Lighting pass
class LightingPass : public RenderPass {
public:
    LightingPass();
    
    void execute() override;
    
    // Set G-Buffer
    void set_gbuffer(std::shared_ptr<RenderTarget> gbuffer) { gbuffer_ = gbuffer; }
    
    // Set output target
    void set_output_target(std::shared_ptr<RenderTarget> target) { output_target_ = target; }

private:
    std::shared_ptr<RenderTarget> gbuffer_;
    std::shared_ptr<RenderTarget> output_target_;
};

// Post-process pass
class PostProcessPass : public RenderPass {
public:
    PostProcessPass();
    
    void execute() override;
    
    // Set input texture
    void set_input_texture(uint32_t texture) { input_texture_ = texture; }
    
    // Set output target
    void set_output_target(std::shared_ptr<RenderTarget> target) { output_target_ = target; }
    
    // Post-process effects
    void set_bloom_enabled(bool enabled) { bloom_enabled_ = enabled; }
    void set_tone_mapping_enabled(bool enabled) { tone_mapping_enabled_ = enabled; }
    void set_fxaa_enabled(bool enabled) { fxaa_enabled_ = enabled; }
    void set_exposure(float exposure) { exposure_ = exposure; }

private:
    uint32_t input_texture_ = 0;
    std::shared_ptr<RenderTarget> output_target_;
    bool bloom_enabled_ = false;
    bool tone_mapping_enabled_ = true;
    bool fxaa_enabled_ = false;
    float exposure_ = 1.0f;
};

// UI pass
class UIPass : public RenderPass {
public:
    UIPass();
    
    void execute() override;

private:
    // UI rendering implementation
};

// Render pipeline
class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();
    
    // Initialize pipeline
    void initialize(uint32_t width, uint32_t height);
    
    // Add render pass
    void add_pass(std::unique_ptr<RenderPass> pass);
    
    // Remove render pass
    void remove_pass(const std::string& name);
    
    // Get render pass
    RenderPass* get_pass(const std::string& name);
    
    // Execute all passes
    void execute();
    
    // Resize
    void resize(uint32_t width, uint32_t height);
    
    // Get render target
    RenderTarget* get_render_target(const std::string& name);
    
    // Create render target
    RenderTarget* create_render_target(const std::string& name, uint32_t width, uint32_t height);

private:
    std::vector<std::unique_ptr<RenderPass>> passes_;
    std::unordered_map<std::string, std::unique_ptr<RenderTarget>> render_targets_;
    uint32_t width_;
    uint32_t height_;
};

} // namespace litt
