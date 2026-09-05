// Phase 2: Rasterization Pipeline Implementation

#pragma once

#include "litt_math.h"
#include "litt_material.h"
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace litt {

// Rasterization Pipeline State
struct RasterState {
    enum class PolygonMode {
        FILL,
        LINE,
        POINT
    };
    
    enum class CullMode {
        NONE,
        FRONT,
        BACK
    };
    
    enum class DepthFunc {
        NEVER,
        LESS,
        EQUAL,
        LESS_OR_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_OR_EQUAL,
        ALWAYS
    };
    
    bool depth_test = true;
    bool depth_write = true;
    CullMode cull_mode = CullMode::BACK;
    PolygonMode polygon_mode = PolygonMode::FILL;
    float line_width = 1.0f;
    int point_size = 1;
    DepthFunc depth_func = DepthFunc::LESS;
};

// Rasterizer - Main rasterization pipeline
class Rasterizer {
public:
    Rasterizer(const RasterState& state = RasterState());
    
    // Render a triangle with material
    void render_triangle(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                        const Vec3& n1, const Vec3& n2, const Vec3& n3,
                        const Vec3& color = Vec3::one());
    
    // Render triangle with PBR material
    void render_triangle_pbr(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                            const Vec3& n1, const Vec3& n2, const Vec3& n3,
                            const PBRMaterial& material);
    
    // Get rendered pixels
    const std::vector<Vec3>& get_pixels() const { return pixels_; }
    const std::vector<float>& get_depth_buffer() const { return depth_buffer_; }
    
    // Clear buffers
    void clear(const Vec3& color = Vec3(0.0f, 0.0f, 0.0f));
    
    // Set viewport
    void set_viewport(int x, int y, int width, int height);
    
    // Get current viewport
    void get_viewport(int& x, int& y, int& width, int& height) const;

private:
    RasterState state_;
    std::vector<Vec3> pixels_;
    std::vector<float> depth_buffer_;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    int viewport_width_ = 800;
    int viewport_height_ = 600;
    
    // Helper methods
    void rasterize_triangle(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                           const Vec3& color);
    void rasterize_triangle_pbr(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                               const Vec3& n1, const Vec3& n2, const Vec3& n3,
                               const PBRMaterial& material);
    
    // Interpolation helpers
    float interpolate(float a, float b, float t) const { return a + (b - a) * t; }
    Vec3 interpolate(const Vec3& a, const Vec3& b, float t) const {
        return a + (b - a) * t;
    }
    
    // Point in triangle test
    bool point_in_triangle(const Vec3& P, const Vec3& A, const Vec3& B, const Vec3& C) const;
    
    // Bounding box calculation
    void compute_triangle_bounds(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                int& min_x, int& max_x, int& min_y, int& max_y) const;
    
    // Depth testing
    bool depth_test_pass(float new_depth, int x, int y) const;
    void write_depth(int x, int y, float depth);
    
    // Material sampling helpers
    Vec3 sample_albedo(const PBRMaterial& material) const;
    Vec3 sample_normal(const PBRMaterial& material) const;
    float sample_metallic(const PBRMaterial& material) const;
    float sample_roughness(const PBRMaterial& material) const;
    float sample_ao(const PBRMaterial& material) const;
    Vec3 sample_emission(const PBRMaterial& material) const;
};

// Screen Space Rendering Effects
class SSR {
public:
    void apply_screen_space_reflections(const std::vector<Vec3>& scene_pixels,
                                       const std::vector<float>& depth_buffer,
                                       const RasterState& state);
    
    const std::vector<Vec3>& get_reflections() const { return reflections_; }
    void clear_reflections() { reflections_.clear(); }

private:
    std::vector<Vec3> reflections_;
    std::vector<Vec3> current_scene_;
    std::vector<float> current_depth_;
};

// Frame Buffer - Off-screen rendering
class FrameBuffer {
public:
    FrameBuffer(int width, int height);
    
    void bind() const;
    void unbind() const;
    
    void resize(int width, int height);
    
    const std::vector<Vec3>& get_color_buffer() const { return color_attachments_[0]; }
    const std::vector<float>& get_depth_buffer() const { return depth_buffer_; }
    
    // Multiple buffers for MRT
    void add_color_attachment();
    void add_depth_attachment();
    
    size_t get_color_attachment_count() const { return color_attachments_.size(); }

private:
    int width_;
    int height_;
    std::vector<std::vector<Vec3>> color_attachments_;
    std::vector<float> depth_buffer_;
    bool is_bound_ = false;
};

// Rasterization Helper Utilities
class RasterUtils {
public:
    // Line rasterization (Bresenham's algorithm)
    static void rasterize_line(int x0, int y0, int x1, int y1,
                              std::vector<Vec3>& pixels, const Vec3& color);
    
    // Circle rasterization
    static void rasterize_circle(int center_x, int center_y, int radius,
                                std::vector<Vec3>& pixels, const Vec3& color);
    
    // Anti-aliasing - supersampling
    static void supersample_triangle(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                    std::vector<Vec3>& pixels, const Vec3& color,
                                    int samples_per_pixel = 4);
    
    // Coordinate space conversions
    static Vec3 world_to_screen(const Vec3& world_pos,
                               const float model_matrix[16],
                               const float view_matrix[16],
                               const float projection_matrix[16],
                               int viewport_x, int viewport_y,
                               int viewport_width, int viewport_height);
    
    static Vec3 screen_to_world(const Vec3& screen_pos,
                               const float inv_model_matrix[16],
                               const float inv_view_matrix[16],
                               const float inv_projection_matrix[16]);
};

} // namespace litt