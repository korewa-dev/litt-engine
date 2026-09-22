// LittRenderer - Complete rendering system
// Uses unified types from litt_math.h (Vec3, Vec2, Aabb, etc.)
// Lighting types from litt_lighting.h, textures from litt_texture.h,
// render passes from litt_render_pass.h.

#pragma once
#include <utility>
#include "litt_math.h"
#include "litt_ecs.h"
#include "litt_lighting.h"
#include "litt_texture.h"
#include "litt_material.h"
#include "litt_render_pass.h"
#include "litt_dither.h"
#include "litt_gpu_software.h"
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
    Software,
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
    Vec3 albedo = Vec3::one();
    float roughness = 0.5f;
    float metalness = 0.0f;
    float occlusion = 1.0f;
    float emission = 0.0f;
    Vec3 emission_color = Vec3::zero();
    std::string texture_path;
    std::shared_ptr<void> texture;
    bool transparent = false;
    bool double_sided = false;

    // Dither3D settings
    DitherMaterial dither;

    // Canonical bridge from authored/runtime PBR data into the render-facing
    // material. Keeping this conversion here prevents scene/import code from
    // silently dropping PBR fields before a backend sees them.
    static RenderMaterial from_pbr(const PBRMaterial& source,
                                   const std::string& material_name = {}) {
        RenderMaterial out;
        out.name = material_name;
        out.albedo = source.albedo;
        out.roughness = std::clamp(source.roughness, 0.0f, 1.0f);
        out.metalness = std::clamp(source.metallic, 0.0f, 1.0f);
        out.occlusion = std::clamp(source.ao, 0.0f, 1.0f);
        out.emission_color = source.emission;
        out.emission = source.emission.length();
        out.texture_path = source.albedo_map;
        out.transparent = source.opacity < 0.999f;
        if (out.transparent) out.type = MaterialType::Transparent;
        else if (out.emission > MATH_EPS) out.type = MaterialType::Emissive;
        return out;
    }
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

    Mat4 view = Mat4::identity();
    Mat4 projection = Mat4::identity();
    Mat4 view_projection = Mat4::identity();

    void update() {
        view = Mat4::look_at(position, target, up);
        // perspective() expects degrees and converts internally
        projection = Mat4::perspective(fov, aspect, near_plane, far_plane);
        view_projection = projection * view;
    }

    Vec3 get_forward() const {
        return (target - position).normalized();
    }

    Vec3 get_right() const {
        return Vec3::up().cross(get_forward()).normalized();
    }

    Vec3 get_up() const {
        return get_forward().cross(get_right()).normalized();
    }
};

// =============================================================================
// Render Pass (uses RenderPass from litt_render_pass.h)
// =============================================================================

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

    virtual void clear(Vec3 color, float depth = 1.0f, uint32_t stencil = 0) = 0;
    virtual void set_camera(const RenderCamera& camera) = 0;
    virtual void draw_mesh(const RenderMesh& mesh, const Mat4& transform, const RenderMaterial& material) = 0;
    virtual void draw_line(const Vec3& start, const Vec3& end, Vec3 color) = 0;
    virtual void draw_gizmo(const Vec3& pos, const Vec3& rot, float scale) = 0;

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
    std::shared_ptr<Light> light;
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
    std::vector<std::shared_ptr<Light>> lights;
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
    ~Renderer() override { shutdown(); }

    bool initialize(uint32_t width, uint32_t height, RenderBackend backend) override {
        shutdown();
        if (backend != RenderBackend::Software) {
            backend_ = backend;
            width_ = width;
            height_ = height;
            return false;
        }
        if (!software_.set_framebuffer_size(width, height)) return false;
        if (!software_.initialize("headless")) return false;
        backend_ = RenderBackend::Software;
        width_ = width;
        height_ = height;
        initialized_ = true;
        current_camera_.aspect = height ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        current_camera_.update();
        return true;
    }

    void shutdown() override {
        software_.shutdown();
        initialized_ = false;
        lights_.clear();
        cameras_.clear();
        current_camera_set_ = false;
    }

    void begin_frame() override {}
    void end_frame() override {}
    void present() override { if (initialized_) software_.present(); }

    void clear(Vec3 color, float depth = 1.0f, uint32_t stencil = 0) override {
        (void)depth;
        (void)stencil;
        if (!initialized_) return;
        auto channel = [](float value) -> uint32_t {
            if (!std::isfinite(value)) return 0u;
            value = std::clamp(value, 0.0f, 1.0f);
            return static_cast<uint32_t>(value * 255.0f + 0.5f);
        };
        const uint32_t packed = (channel(color.x) << 16) |
                                (channel(color.y) << 8) |
                                channel(color.z);
        software_.clear(packed);
    }

    void set_camera(const RenderCamera& camera) override {
        current_camera_ = camera;
        current_camera_.update();
        current_camera_set_ = true;
    }

    void draw_mesh(const RenderMesh& mesh, const Mat4& transform,
                   const RenderMaterial& material) override {
        if (!initialized_ || mesh.data.positions.empty() || mesh.data.indices.empty()) return;
        RenderCamera camera = current_camera_;
        if (!current_camera_set_) camera.update();
        const Mat4 view_projection = camera.view_projection * transform;
        software_.draw_mesh(mesh.data.positions, mesh.data.indices, view_projection,
                            pack_color(material.albedo));
    }

    void draw_line(const Vec3& start, const Vec3& end, Vec3 color) override {
        if (!initialized_) return;
        RenderCamera camera = current_camera_;
        if (!current_camera_set_) camera.update();
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float z0 = 1.0f, z1 = 1.0f;
        if (software_.project_checked(start, camera.view_projection, x0, y0, z0) &&
            software_.project_checked(end, camera.view_projection, x1, y1, z1)) {
            software_.draw_line_3d(x0, y0, z0, x1, y1, z1, pack_color(color));
        }
    }

    void draw_gizmo(const Vec3& pos, const Vec3&, float scale) override {
        if (!std::isfinite(scale) || scale <= 0.0f) return;
        const float len = scale;
        draw_line(pos, pos + Vec3(len, 0, 0), Vec3(1, 0, 0));
        draw_line(pos, pos + Vec3(0, len, 0), Vec3(0, 1, 0));
        draw_line(pos, pos + Vec3(0, 0, len), Vec3(0, 0, 1));
    }

    uint32_t get_width() const override { return width_; }
    uint32_t get_height() const override { return height_; }

    void* get_native_handle() override { return software_.get_window(); }

    std::string get_backend_name() const override {
        switch (backend_) {
            case RenderBackend::Software: return "Software";
            case RenderBackend::Vulkan: return "Vulkan (unavailable)";
            case RenderBackend::DirectX12: return "DirectX 12 (unavailable)";
            case RenderBackend::OpenGL: return "OpenGL (unavailable)";
            case RenderBackend::Metal: return "Metal (unavailable)";
        }
        return "Unknown";
    }

    bool initialized() const { return initialized_; }
    const std::vector<uint8_t>& framebuffer_pixels() const {
        return software_.framebuffer_pixels();
    }
    uint32_t get_pixel(int x, int y) const { return software_.get_pixel(x, y); }

    void render_scene(const RenderScene& scene) {
        if (scene.root) render_node(*scene.root);
    }

    void render_node(const RenderNode& node) {
        if (!node.visible) return;
        if (node.mesh && node.material) {
            draw_mesh(*node.mesh, node.transform.matrix, *node.material);
        }
        for (const auto& child : node.children) {
            if (child) render_node(*child);
        }
    }

    void add_light(const std::shared_ptr<Light>& light) {
        if (light) lights_.push_back(light);
    }

    void add_camera(const std::shared_ptr<RenderCamera>& camera) {
        if (!camera) return;
        cameras_.push_back(camera);
        if (!current_camera_set_) set_camera(*camera);
    }

private:
    static uint32_t pack_color(const Vec3& color) {
        auto channel = [](float value) -> uint32_t {
            if (!std::isfinite(value)) return 0u;
            value = std::clamp(value, 0.0f, 1.0f);
            return static_cast<uint32_t>(value * 255.0f + 0.5f);
        };
        return (channel(color.x) << 16) | (channel(color.y) << 8) | channel(color.z);
    }

    SoftwareRenderer software_;
    RenderBackend backend_ = RenderBackend::Software;
    uint32_t width_ = 800;
    uint32_t height_ = 600;
    bool initialized_ = false;
    RenderCamera current_camera_;
    bool current_camera_set_ = false;
    std::vector<std::shared_ptr<Light>> lights_;
    std::vector<std::shared_ptr<RenderCamera>> cameras_;
};

} // namespace litt
