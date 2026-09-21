// Software Renderer - portable headless framebuffer with optional Win32 GDI presentation
// The rasterizer is platform-neutral. Window creation/presentation is compiled only on Windows.

#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include "litt_gpu.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace litt {

namespace software_detail {

static constexpr size_t MAX_PIXELS = 16u * 1024u * 1024u;
static constexpr size_t MAX_BUFFER_BYTES = 64u * 1024u * 1024u;

inline bool image_sizes(uint32_t width, uint32_t height, size_t& pixels, size_t& bytes) {
    if (width != 0 && static_cast<size_t>(height) >
        std::numeric_limits<size_t>::max() / static_cast<size_t>(width)) {
        return false;
    }
    pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixels == 0 || pixels > MAX_PIXELS ||
        pixels > std::numeric_limits<size_t>::max() / 4u) return false;
    bytes = pixels * 4u;
    return true;
}

} // namespace software_detail

class SoftwareRenderer : public IGPUDevice {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override { shutdown(); }
    
    bool initialize(const std::string& adapter_name = "") override {
        if (initialized_) return true;
        
        // Headless mode for testing - no window
        if (adapter_name == "headless") {
            if (!resize_buffers()) return false;
            
#ifdef _WIN32
            bmp_info_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmp_info_.bmiHeader.biWidth = width_;
            bmp_info_.bmiHeader.biHeight = -height_;
            bmp_info_.bmiHeader.biPlanes = 1;
            bmp_info_.bmiHeader.biBitCount = 32;
            bmp_info_.bmiHeader.biCompression = BI_RGB;
#endif
            
            initialized_ = true;
            return true;
        }
        
#ifdef _WIN32
        // Register window class
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "LittEngineSW";
        RegisterClassEx(&wc);
        
        // Create window
        hwnd_ = CreateWindowEx(
            0, "LittEngineSW", "Litt Engine (Software)",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            width_, height_, nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );
        
        if (!hwnd_) return false;
        
        hdc_ = GetDC(hwnd_);
        
        // Allocate framebuffer with checked size arithmetic.
        if (!resize_buffers()) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
            hdc_ = nullptr;
            return false;
        }
        
        // Setup bitmap info
        bmp_info_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmp_info_.bmiHeader.biWidth = width_;
        bmp_info_.bmiHeader.biHeight = -height_; // Top-down
        bmp_info_.bmiHeader.biPlanes = 1;
        bmp_info_.bmiHeader.biBitCount = 32;
        bmp_info_.bmiHeader.biCompression = BI_RGB;
        
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        
        std::cout << "[SW] Software renderer initialized: " << width_ << "x" << height_ << std::endl;
#else
        (void)adapter_name;
        return false;
#endif
        initialized_ = true;
        return true;
    }
    
    void shutdown() override {
#ifdef _WIN32
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        hdc_ = nullptr;
#endif
        initialized_ = false;
    }
    
    void present() override {
#ifdef _WIN32
        // Headless mode intentionally has no device context.
        if (!hdc_ || framebuffer_.empty()) return;
        SetDIBitsToDevice(
            hdc_, 0, 0, width_, height_,
            0, 0, 0, height_,
            framebuffer_.data(), &bmp_info_, DIB_RGB_COLORS
        );
#endif
    }
    
    std::unique_ptr<GPUBuffer> create_buffer(const BufferDesc& desc) override {
        if (desc.size == 0 || desc.size > software_detail::MAX_BUFFER_BYTES) return nullptr;
        return std::make_unique<SoftwareBuffer>(desc);
    }
    
    void update_buffer(GPUBuffer* buffer, const void* data, size_t size) override {
        if (buffer) buffer->update(data, size);
    }
    void destroy_buffer(GPUBuffer* /*buffer*/) override {}
    
    std::unique_ptr<GPUTexture> create_texture(const TextureDesc& desc) override {
        if (desc.width == 0 || desc.height == 0 || desc.format != TextureFormat::RGBA8) {
            return nullptr;
        }
        size_t pixels = 0, bytes = 0;
        if (!software_detail::image_sizes(desc.width, desc.height, pixels, bytes)) return nullptr;
        return std::make_unique<SoftwareTexture>(desc);
    }
    
    void destroy_texture(GPUTexture* /*texture*/) override {}
    
    std::unique_ptr<GPUShader> create_shader(const std::string& /*vs*/, const std::string& /*fs*/) override {
        return nullptr;
    }
    
    std::unique_ptr<GPUShader> create_shader_from_file(const std::string& /*path*/) override {
        return nullptr;
    }
    
    std::shared_ptr<RenderTarget> create_render_target(uint32_t width, uint32_t height) override {
        return nullptr;
    }
    
    size_t get_vram_usage() const override { return 0; }
    
    // === 2D Drawing API ===
    
    void clear(uint32_t color) {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        
        for (uint32_t y = 0; y < height_; y++) {
            for (uint32_t x = 0; x < width_; x++) {
                uint32_t idx = (y * width_ + x) * 4;
                framebuffer_[idx + 0] = b;
                framebuffer_[idx + 1] = g;
                framebuffer_[idx + 2] = r;
                framebuffer_[idx + 3] = 255;
                depth_buffer_[y * width_ + x] = 1.0f;
            }
        }
    }
    
    void draw_pixel(int x, int y, uint32_t color) {
        if (x < 0 || x >= (int)width_ || y < 0 || y >= (int)height_) return;
        uint32_t idx = (y * width_ + x) * 4;
        framebuffer_[idx + 0] = (color >> 0) & 0xFF;
        framebuffer_[idx + 1] = (color >> 8) & 0xFF;
        framebuffer_[idx + 2] = (color >> 16) & 0xFF;
        framebuffer_[idx + 3] = 255;
    }
    
    void draw_pixel_depth(int x, int y, float depth, uint32_t color) {
        if (x < 0 || x >= (int)width_ || y < 0 || y >= (int)height_) return;
        if (!std::isfinite(depth)) return;
        uint32_t idx = y * width_ + x;
        if (depth < 0.0f || depth > 1.0f || depth >= depth_buffer_[idx]) return;
        depth_buffer_[idx] = depth;
        uint32_t pidx = idx * 4;
        framebuffer_[pidx + 0] = (color >> 0) & 0xFF;
        framebuffer_[pidx + 1] = (color >> 8) & 0xFF;
        framebuffer_[pidx + 2] = (color >> 16) & 0xFF;
        framebuffer_[pidx + 3] = 255;
    }
    
    void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        
        while (true) {
            draw_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    
    void draw_rect(int x, int y, int w, int h, uint32_t color) {
        draw_line(x, y, x + w, y, color);
        draw_line(x + w, y, x + w, y + h, color);
        draw_line(x + w, y + h, x, y + h, color);
        draw_line(x, y + h, x, y, color);
    }
    
    void fill_rect(int x, int y, int w, int h, uint32_t color) {
        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                draw_pixel(px, py, color);
            }
        }
    }
    
    void draw_text(int x, int y, const std::string& text, uint32_t color) {
#ifdef _WIN32
        // GDI text is only available for a window-backed renderer. Headless
        // mode deliberately has no device context, so treat text as a no-op
        // instead of passing a null HDC into Win32.
        if (!hdc_ || text.empty()) return;
        SetTextColor(hdc_, RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
        SetBkMode(hdc_, TRANSPARENT);
        TextOutA(hdc_, x, y, text.c_str(), static_cast<int>(text.size()));
#else
        (void)x; (void)y; (void)text; (void)color;
#endif
    }
    
    // === 3D Triangle Rasterization ===
    
    // Draw a filled triangle with depth testing
    void draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
        // Sort vertices by y
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
        
        int total_height = y2 - y0;
        if (total_height == 0) return;
        
        for (int i = 0; i < total_height; i++) {
            bool second_half = i > (y1 - y0) || y1 == y0;
            int segment_height = second_half ? (y2 - y1) : (y1 - y0);
            if (segment_height == 0) continue;
            
            float alpha = (float)i / total_height;
            float beta = (float)(i - (second_half ? (y1 - y0) : 0)) / segment_height;
            
            int ax = x0 + (x2 - x0) * alpha;
            int bx = second_half ? (x1 + (x2 - x1) * beta) : (x0 + (x1 - x0) * beta);
            
            if (ax > bx) std::swap(ax, bx);
            
            for (int x = ax; x <= bx; x++) {
                draw_pixel(x, y0 + i, color);
            }
        }
    }
    
    // Draw a filled triangle with depth interpolation
    void draw_triangle_depth(
        int x0, int y0, float z0,
        int x1, int y1, float z1,
        int x2, int y2, float z2,
        uint32_t color) {
        // Sort vertices by y
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); std::swap(z0, z1); }
        if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); std::swap(z0, z2); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); std::swap(z1, z2); }
        
        int total_height = y2 - y0;
        if (total_height == 0) return;
        
        for (int i = 0; i < total_height; i++) {
            bool second_half = i > (y1 - y0) || y1 == y0;
            int segment_height = second_half ? (y2 - y1) : (y1 - y0);
            if (segment_height == 0) continue;
            
            float alpha = (float)i / total_height;
            float beta = (float)(i - (second_half ? (y1 - y0) : 0)) / segment_height;
            
            int ax = x0 + (x2 - x0) * alpha;
            int bx = second_half ? (x1 + (x2 - x1) * beta) : (x0 + (x1 - x0) * beta);
            
            float az = z0 + (z2 - z0) * alpha;
            float bz = second_half ? (z1 + (z2 - z1) * beta) : (z0 + (z1 - z0) * beta);
            
            if (ax > bx) {
                std::swap(ax, bx);
                std::swap(az, bz);
            }
            
            for (int x = ax; x <= bx; x++) {
                float t = (bx == ax) ? 0.0f : (float)(x - ax) / (bx - ax);
                float z = az + (bz - az) * t;
                draw_pixel_depth(x, y0 + i, z, color);
            }
        }
    }
    
    // Draw a 3D line with depth
    void draw_line_3d(int x0, int y0, float z0, int x1, int y1, float z1, uint32_t color) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        int total_steps = dx + abs(dy);
        if (total_steps == 0) total_steps = 1;
        int step = 0;
        
        while (true) {
            float t = (float)step / total_steps;
            float z = z0 + (z1 - z0) * t;
            draw_pixel_depth(x0, y0, z, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
            step++;
        }
    }
    
    // Project 3D point to 2D screen coordinates. Points outside the near/far
    // clip range are explicitly rejected instead of forcing an invalid w.
    bool project_checked(const Vec3& world_pos, const Mat4& view_proj,
                         int& screen_x, int& screen_y, float& depth) const {
        const Vec4 clip = view_proj * Vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
        if (clip.w <= kClipEpsilon || clip.z < -clip.w || clip.z > clip.w) {
            screen_x = screen_y = -1;
            depth = 1.0f;
            return false;
        }
        return project_clip_vertex(clip, screen_x, screen_y, depth);
    }

    void project(const Vec3& world_pos, const Mat4& view_proj,
                 int& screen_x, int& screen_y, float& depth) {
        (void)project_checked(world_pos, view_proj, screen_x, screen_y, depth);
    }
    
    // Draw a 3D triangle from world coordinates. Clip in homogeneous space
    // before perspective division so triangles crossing the near plane remain
    // well behaved and triangles behind the camera are rejected.
    void draw_triangle_3d(
        const Vec3& v0, const Vec3& v1, const Vec3& v2,
        const Mat4& view_proj, uint32_t color) {
        std::vector<Vec4> polygon = {
            view_proj * Vec4(v0.x, v0.y, v0.z, 1.0f),
            view_proj * Vec4(v1.x, v1.y, v1.z, 1.0f),
            view_proj * Vec4(v2.x, v2.y, v2.z, 1.0f)
        };

        clip_polygon(polygon, 0); // w > 0
        clip_polygon(polygon, 1); // OpenGL near plane: z >= -w
        clip_polygon(polygon, 2); // far plane: z <= w
        if (polygon.size() < 3) return;

        for (size_t i = 1; i + 1 < polygon.size(); ++i) {
            int x0, y0, x1, y1, x2, y2;
            float z0, z1, z2;
            if (!project_clip_vertex(polygon[0], x0, y0, z0) ||
                !project_clip_vertex(polygon[i], x1, y1, z1) ||
                !project_clip_vertex(polygon[i + 1], x2, y2, z2)) {
                continue;
            }
            draw_triangle_depth(x0, y0, z0, x1, y1, z1, x2, y2, z2, color);
        }
    }
    
    // Draw a 3D mesh
    void draw_mesh(const std::vector<Vec3>& vertices, const std::vector<uint32_t>& indices,
                   const Mat4& view_proj, uint32_t color) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const uint32_t i0 = indices[i];
            const uint32_t i1 = indices[i + 1];
            const uint32_t i2 = indices[i + 2];
            // Asset data is not trusted. A malformed mesh must not turn into
            // an out-of-bounds read in the software fallback renderer.
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
                continue;
            }
            draw_triangle_3d(vertices[i0], vertices[i1], vertices[i2], view_proj, color);
        }
    }
    
    // Draw a 3D cube
    void draw_cube(const Vec3& center, float size, const Mat4& view_proj, uint32_t color) {
        float s = size * 0.5f;
        std::vector<Vec3> verts = {
            {center.x - s, center.y - s, center.z - s},
            {center.x + s, center.y - s, center.z - s},
            {center.x + s, center.y + s, center.z - s},
            {center.x - s, center.y + s, center.z - s},
            {center.x - s, center.y - s, center.z + s},
            {center.x + s, center.y - s, center.z + s},
            {center.x + s, center.y + s, center.z + s},
            {center.x - s, center.y + s, center.z + s},
        };
        
        // Front face
        draw_triangle_3d(verts[0], verts[1], verts[2], view_proj, color);
        draw_triangle_3d(verts[0], verts[2], verts[3], view_proj, color);
        // Back face
        draw_triangle_3d(verts[5], verts[4], verts[7], view_proj, color);
        draw_triangle_3d(verts[5], verts[7], verts[6], view_proj, color);
        // Top face
        draw_triangle_3d(verts[4], verts[5], verts[1], view_proj, color);
        draw_triangle_3d(verts[4], verts[1], verts[0], view_proj, color);
        // Bottom face
        draw_triangle_3d(verts[3], verts[2], verts[6], view_proj, color);
        draw_triangle_3d(verts[3], verts[6], verts[7], view_proj, color);
        // Left face
        draw_triangle_3d(verts[4], verts[0], verts[3], view_proj, color);
        draw_triangle_3d(verts[4], verts[3], verts[7], view_proj, color);
        // Right face
        draw_triangle_3d(verts[1], verts[5], verts[6], view_proj, color);
        draw_triangle_3d(verts[1], verts[6], verts[2], view_proj, color);
    }
    
    // Draw a grid (for ground plane)
    void draw_grid(float spacing, const Mat4& view_proj, uint32_t color) {
        // Prevent a zero/negative/NaN spacing from producing an infinite loop.
        if (!std::isfinite(spacing) || spacing <= 0.0f) return;
        for (float x = -10.0f; x <= 10.0f; x += spacing) {
            Vec3 start(x, 0, -10);
            Vec3 end(x, 0, 10);
            int x0, y0, x1, y1;
            float z0, z1;
            if (project_checked(start, view_proj, x0, y0, z0) &&
                project_checked(end, view_proj, x1, y1, z1)) {
                draw_line_3d(x0, y0, z0, x1, y1, z1, color);
            }
        }
        for (float z = -10.0f; z <= 10.0f; z += spacing) {
            Vec3 start(-10, 0, z);
            Vec3 end(10, 0, z);
            int x0, y0, x1, y1;
            float z0, z1;
            if (project_checked(start, view_proj, x0, y0, z0) &&
                project_checked(end, view_proj, x1, y1, z1)) {
                draw_line_3d(x0, y0, z0, x1, y1, z1, color);
            }
        }
    }
    
    // Draw a heightmap terrain
    void draw_terrain(const std::vector<float>& heightmap, uint32_t size, 
                      float scale, const Mat4& view_proj, uint32_t color) {
        if (size < 2) return;
        size_t required = 0, ignored_bytes = 0;
        if (!software_detail::image_sizes(size, size, required, ignored_bytes) ||
            heightmap.size() < required) return;
        
        for (uint32_t z = 0; z < size - 1; z++) {
            for (uint32_t x = 0; x < size - 1; x++) {
                float h00 = heightmap[z * size + x];
                float h10 = heightmap[z * size + (x + 1)];
                float h01 = heightmap[(z + 1) * size + x];
                float h11 = heightmap[(z + 1) * size + (x + 1)];
                
                Vec3 v00((static_cast<float>(x) - static_cast<float>(size) * 0.5f) * scale, h00 * scale, (static_cast<float>(z) - static_cast<float>(size) * 0.5f) * scale);
                Vec3 v10((static_cast<float>(x) + 1.0f - static_cast<float>(size) * 0.5f) * scale, h10 * scale, (static_cast<float>(z) - static_cast<float>(size) * 0.5f) * scale);
                Vec3 v01((static_cast<float>(x) - static_cast<float>(size) * 0.5f) * scale, h01 * scale, (static_cast<float>(z) + 1.0f - static_cast<float>(size) * 0.5f) * scale);
                Vec3 v11((static_cast<float>(x) + 1.0f - static_cast<float>(size) * 0.5f) * scale, h11 * scale, (static_cast<float>(z) + 1.0f - static_cast<float>(size) * 0.5f) * scale);
                
                draw_triangle_3d(v00, v10, v01, view_proj, color);
                draw_triangle_3d(v10, v11, v01, view_proj, color);
            }
        }
    }
    
#ifdef _WIN32
    HWND get_window() const { return hwnd_; }
#else
    void* get_window() const { return nullptr; }
#endif

    uint32_t get_pixel(int x, int y) const {
        if (x < 0 || x >= static_cast<int>(width_) ||
            y < 0 || y >= static_cast<int>(height_) || framebuffer_.empty()) {
            return 0;
        }
        const size_t idx = (static_cast<size_t>(y) * width_ + static_cast<size_t>(x)) * 4u;
        const uint32_t b = framebuffer_[idx + 0];
        const uint32_t g = framebuffer_[idx + 1];
        const uint32_t r = framebuffer_[idx + 2];
        return (r << 16) | (g << 8) | b;
    }

    const std::vector<uint8_t>& framebuffer_pixels() const { return framebuffer_; }
    
private:
    static constexpr float kClipEpsilon = 1e-5f;

    bool resize_buffers() {
        size_t pixels = 0, bytes = 0;
        if (!software_detail::image_sizes(width_, height_, pixels, bytes)) return false;
        framebuffer_.assign(bytes, 0);
        depth_buffer_.assign(pixels, 1.0f);
        return true;
    }

    static float clip_distance(const Vec4& v, int plane) {
        switch (plane) {
            case 0: return v.w - kClipEpsilon;
            case 1: return v.z + v.w;
            default: return v.w - v.z;
        }
    }

    static void clip_polygon(std::vector<Vec4>& polygon, int plane) {
        if (polygon.empty()) return;
        std::vector<Vec4> output;
        output.reserve(polygon.size() + 1);

        Vec4 previous = polygon.back();
        float previous_distance = clip_distance(previous, plane);
        bool previous_inside = previous_distance >= 0.0f;

        for (const Vec4& current : polygon) {
            const float current_distance = clip_distance(current, plane);
            const bool current_inside = current_distance >= 0.0f;

            if (current_inside != previous_inside) {
                const float denom = previous_distance - current_distance;
                if (std::fabs(denom) > kClipEpsilon) {
                    const float t = previous_distance / denom;
                    output.push_back(previous + (current - previous) * t);
                }
            }
            if (current_inside) output.push_back(current);

            previous = current;
            previous_distance = current_distance;
            previous_inside = current_inside;
        }
        polygon.swap(output);
    }

    bool project_clip_vertex(const Vec4& clip, int& screen_x, int& screen_y,
                             float& depth) const {
        if (clip.w <= kClipEpsilon) return false;
        const float ndc_x = clip.x / clip.w;
        const float ndc_y = clip.y / clip.w;
        const float ndc_z = clip.z / clip.w;
        if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) {
            return false;
        }

        screen_x = static_cast<int>((ndc_x * 0.5f + 0.5f) * width_);
        screen_y = static_cast<int>((-ndc_y * 0.5f + 0.5f) * height_);
        depth = std::clamp(ndc_z * 0.5f + 0.5f, 0.0f, 1.0f);
        return true;
    }

    class SoftwareBuffer : public GPUBuffer {
    public:
        explicit SoftwareBuffer(const BufferDesc& desc)
            : bytes_(desc.size), usage_(desc.usage) {
            if (desc.data && desc.size) {
                std::memcpy(bytes_.data(), desc.data, desc.size);
            }
        }

        void update(const void* data, size_t size) override {
            if (!data || size > bytes_.size()) return;
            std::memcpy(bytes_.data(), data, size);
        }
        void* map() override { return bytes_.empty() ? nullptr : bytes_.data(); }
        void unmap() override {}
        size_t get_size() const override { return bytes_.size(); }
        GPUBufferUsage get_type() const override { return usage_; }

    private:
        std::vector<uint8_t> bytes_;
        GPUBufferUsage usage_;
    };

    class SoftwareTexture : public GPUTexture {
    public:
        SoftwareTexture(const TextureDesc& desc) : desc_(desc) {
            size_t pixels = 0, bytes = 0;
            if (!software_detail::image_sizes(desc.width, desc.height, pixels, bytes)) {
                throw std::length_error("software texture dimensions overflow");
            }
            pixels_.resize(bytes);
        }
        
        void update(const void* data, uint32_t width, uint32_t height) override {
            if (!data || width == 0 || height == 0) return;
            size_t pixels = 0, bytes = 0;
            if (!software_detail::image_sizes(width, height, pixels, bytes)) return;
            if (width != desc_.width || height != desc_.height) {
                desc_.width = width;
                desc_.height = height;
                pixels_.resize(bytes);
            }
            memcpy(pixels_.data(), data, bytes);
        }
        
        uint32_t get_width() const override { return desc_.width; }
        uint32_t get_height() const override { return desc_.height; }
        TextureFormat get_format() const override { return desc_.format; }
        
        std::vector<uint8_t> pixels_;
        TextureDesc desc_;
    };
    
#ifdef _WIN32
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    BITMAPINFO bmp_info_ = {};
#endif
    std::vector<uint8_t> framebuffer_;
    std::vector<float> depth_buffer_;
    uint32_t width_ = 800;
    uint32_t height_ = 600;
    bool initialized_ = false;
};

} // namespace litt
