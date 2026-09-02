// LittRenderer - Complete rendering system
// Uses unified types from litt_math.h (Vec3, Vec2, Aabb, etc.)

#pragma once
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_dither.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace litt {

// =============================================================================
// Render Backends
// =============================================================================

enum class RenderBackend {
    Vulkan,
    DirectX12,
    OpenGL,
    Metal
};

// =============================================================================
// Mesh Data (uses unified Vec3 from litt_math.h)
// =============================================================================

struct MeshData {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;
    std::vector<uint32_t> indices;
    Aabb bounds;
    std::string name;
};

struct RenderMesh {
    MeshData data;
    uint32_t id = 0;
    bool uploaded = false;
};

// =============================================================================
// Material
// =============================================================================

enum class MaterialType {
    Standard,
    Unlit,
    Transparent,
    Emissive
};

struct RenderMaterial {
    std::string name;
    MaterialType type = MaterialType::Standard;
    Vec3f albedo = Vec3f::one();
    float roughness = 0.5f;
    float metalness = 0.0f;
    float occlusion = 1.0f;
    float emission = 0.0f;
    Vec3f emission_color = Vec3f::zero();
    std::string texture_path;
    std::shared_ptr<void> texture;
    bool transparent = false;
    bool double_sided = false;

    // Dither3D settings
    DitherMaterial dither;
};

// =============================================================================
// Light
// =============================================================================

enum class LightType {
    Point,
    Directional,
    Spot,
    Area
};

struct RenderLight {
    LightType type = LightType::Point;
    Vec3 position = Vec3::zero();
    Vec3 direction = Vec3::up();
    Vec3 color = Vec3::one();
    float intensity = 1.0f;
    float range = 10.0f;
    float spot_angle = 45.0f;
    bool cast_shadows = true;
};

// =============================================================================
// Camera
// =============================================================================

struct RenderCamera {
    Vec3 position = Vec3{0, 5, -10};
    Vec3 target = Vec3::zero();
    Vec3 up = Vec3::up();
    float fov = 60.0f;
    float aspect = 16.0f / 9.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    Mat4f view = Mat4f::identity();
    Mat4f projection = Mat4f::identity();
    Mat4f view_projection = Mat4f::identity();

    void update() {
        view = Mat4f::look_at(position, target, up);
        projection = Mat4f::perspective(
            fov * LITT_MATH_DEG2RAD, aspect, near_plane, far_plane);
        view_projection = projection * view;
    }

    Vec3f get_forward() const {
        return (target - position).normalized();
    }

    Vec3f get_right() const {
        return Vec3f::up().cross(get_forward()).normalized();
    }

    Vec3f get_up() const {
        return get_forward().cross(get_right()).normalized();
    }
};

// =============================================================================
// Render Pass
// =============================================================================

struct RenderPass {
    std::string name;
    std::function<void()> begin;
    std::function<void()> end;
    std::function<void(const RenderCamera&)> render;
};

// =============================================================================
// Renderer Interface
// =============================================================================

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(uint32_t width, uint32_t height, RenderBackend backend) = 0;
    virtual void shutdown() = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void present() = 0;

    virtual void clear(Vec3f color, float depth = 1.0f, uint32_t stencil = 0) = 0;
    virtual void set_camera(const RenderCamera& camera) = 0;
    virtual void draw_mesh(const RenderMesh& mesh, const Mat4f& transform, const RenderMaterial& material) = 0;
    virtual void draw_line(const Vec3f& start, const Vec3f& end, Vec3f color) = 0;
    virtual void draw_gizmo(const Vec3f& pos, const Vec3f& rot, float scale) = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;

    // Renderer-specific queries
    virtual void* get_native_handle() = 0;
    virtual std::string get_backend_name() const = 0;
};

// =============================================================================
// Scene
// =============================================================================

struct RenderNode {
    uint32_t id = 0;
    std::string name;
    Transform transform;
    std::vector<std::unique_ptr<RenderNode>> children;
    RenderNode* parent = nullptr;

    // Components
    std::shared_ptr<RenderMesh> mesh;
    std::shared_ptr<RenderMaterial> material;
    std::shared_ptr<RenderLight> light;
    std::shared_ptr<RenderCamera> camera;

    // Visibility
    bool visible = true;
    bool cast_shadows = true;
    bool receive_shadows = true;

    void add_child(std::unique_ptr<RenderNode> child) {
        child->parent = this;
        children.push_back(std::move(child));
    }

    RenderNode* find_child(const std::string& name) {
        for (auto& child : children) {
            if (child->name == name) return child.get();
            auto found = child->find_child(name);
            if (found) return found;
        }
        return nullptr;
    }

    void update_transform() {
        transform.update();
        for (auto& child : children) {
            child->update_transform();
        }
    }
};

class RenderScene {
public:
    std::unique_ptr<RenderNode> root;
    std::vector<std::shared_ptr<RenderLight>> lights;
    std::vector<std::shared_ptr<RenderCamera>> cameras;

    RenderScene() {
        root = std::make_unique<RenderNode>();
        root->name = "Root";
    }

    RenderNode& create_node(const std::string& name) {
        auto node = std::make_unique<RenderNode>();
        node->name = name;
        node->id = next_id_++;
        root->add_child(std::move(node));
        return *root->children.back();
    }

    RenderNode* find_node(const std::string& name) {
        return root->find_child(name);
    }

    void update() {
        root->update_transform();
    }

    void clear() {
        root = std::make_unique<RenderNode>();
        root->name = "Root";
        lights.clear();
        cameras.clear();
    }

    uint32_t next_id_ = 1;
};

// =============================================================================
// Renderer Implementation
// =============================================================================

class Renderer : public IRenderer {
public:
    Renderer() = default;
    ~Renderer() override = default;

    bool initialize(uint32_t width, uint32_t height, RenderBackend backend) override {
        backend_ = backend;
        width_ = width;
        height_ = height;

        // Initialize based on backend
        switch (backend) {
            case RenderBackend::Vulkan:
                return init_vulkan(width, height);
            case RenderBackend::DirectX12:
                return init_dx12(width, height);
            case RenderBackend::OpenGL:
                return init_opengl(width, height);
            case RenderBackend::Metal:
                return init_metal(width, height);
        }
        return false;
    }

    void shutdown() override {
        // Cleanup resources
    }

    void begin_frame() override {
        // Begin rendering
    }

    void end_frame() override {
        // End rendering
    }

    void present() override {
        // Present frame
    }

    void clear(Vec3f, float, uint32_t) override {
        // Clear buffers
    }

    void set_camera(const RenderCamera& camera) override {
        current_camera_ = camera;
    }

    void draw_mesh(const RenderMesh&, const Mat4f&, const RenderMaterial&) override {
        // Draw mesh with transform and material
    }

    void draw_line(const Vec3f&, const Vec3f&, Vec3f) override {
        // Draw line (for debug visualization)
    }

    void draw_gizmo(const Vec3f& pos, const Vec3f&, float scale) override {
        // Draw transform gizmo
        float len = 30.0f * scale;

        // X axis (red)
        draw_line(pos, pos + Vec3f(len, 0, 0), Vec3f(1, 0, 0));
        // Y axis (green)
        draw_line(pos, pos + Vec3f(0, len, 0), Vec3f(0, 1, 0));
        // Z axis (blue)
        draw_line(pos, pos + Vec3f(0, 0, len), Vec3f(0, 0, 1));
    }

    uint32_t get_width() const override { return width_; }
    uint32_t get_height() const override { return height_; }

    void* get_native_handle() override { return nullptr; }
    std::string get_backend_name() const override {
        switch (backend_) {
            case RenderBackend::Vulkan: return "Vulkan";
            case RenderBackend::DirectX12: return "DirectX 12";
            case RenderBackend::OpenGL: return "OpenGL";
            case RenderBackend::Metal: return "Metal";
        }
        return "Unknown";
    }

    // Scene rendering
    void render_scene(const RenderScene& scene) {
        render_node(*scene.root);
    }

    void render_node(const RenderNode& node) {
        if (!node.visible) return;

        // Render this node
        if (node.mesh && node.material) {
            set_camera(current_camera_);
            draw_mesh(*node.mesh, node.transform.matrix, *node.material);
        }

        // Render children
        for (auto& child : node.children) {
            render_node(*child);
        }
    }

    // Add light
    void add_light(std::shared_ptr<RenderLight> light) {
        lights_.push_back(light);
    }

    // Add camera
    void add_camera(std::shared_ptr<RenderCamera> camera) {
        cameras_.push_back(camera);
        if (!current_camera_set_) {
            current_camera_ = *camera;
            current_camera_set_ = true;
        }
    }

private:
    bool init_vulkan(uint32_t, uint32_t) {
        // Vulkan initialization
        return true;
    }

    bool init_dx12(uint32_t, uint32_t) {
        // DirectX 12 initialization
        return false;
    }

    bool init_opengl(uint32_t, uint32_t) {
        // OpenGL initialization
        return false;
    }

    bool init_metal(uint32_t, uint32_t) {
        // Metal initialization
        return false;
    }

    RenderBackend backend_ = RenderBackend::Vulkan;
    uint32_t width_ = 1920;
    uint32_t height_ = 1080;

    RenderCamera current_camera_;
    bool current_camera_set_ = false;

    std::vector<std::shared_ptr<RenderLight>> lights_;
    std::vector<std::shared_ptr<RenderCamera>> cameras_;

    // =============================================================================
    // Dither3D Integration
    // =============================================================================

    DitherMaterial dither_material_;
    DitherAssetManager dither_assets_;

    void enable_dither(DitherColorMode mode = DitherColorMode::Grayscale,
                       DitherPattern pattern = DitherPattern::P8x8) {
        dither_material_.enabled = true;
        dither_material_.color_mode = mode;
        dither_material_.pattern = pattern;
        dither_assets_.generate_textures();
    }

    void disable_dither() {
        dither_material_.enabled = false;
    }

    void set_dither_scale(float scale) {
        dither_material_.scale = scale;
    }

    void set_dither_mode(DitherColorMode mode) {
        dither_material_.color_mode = mode;
    }

    void set_dither_params(float scale, float size_var, float contrast,
                          DitherColorMode mode, DitherPattern pattern) {
        dither_material_.scale = scale;
        dither_material_.size_variability = size_var;
        dither_material_.contrast = contrast;
        dither_material_.color_mode = mode;
        dither_material_.pattern = pattern;
    }
};

} // namespace litt
