// Windows Software Renderer - Real framebuffer output using GDI
// No external dependencies - uses Windows built-in GDI for pixel output

#pragma once
#include "litt_gpu.h"
#include <windows.h>
#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace litt {

class SoftwareRenderer : public IGPUDevice {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override { shutdown(); }
    
    bool initialize(const std::string& adapter_name = "") override {
        if (initialized_) return true;
        
        // Headless mode for testing - no window
        if (adapter_name == "headless") {
            framebuffer_.resize(width_ * height_ * 4);
            depth_buffer_.resize(width_ * height_);
            
            bmp_info_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmp_info_.bmiHeader.biWidth = width_;
            bmp_info_.bmiHeader.biHeight = -height_;
            bmp_info_.bmiHeader.biPlanes = 1;
            bmp_info_.bmiHeader.biBitCount = 32;
            bmp_info_.bmiHeader.biCompression = BI_RGB;
            
            initialized_ = true;
            return true;
        }
        
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
        
        // Allocate framebuffer
        framebuffer_.resize(width_ * height_ * 4);
        depth_buffer_.resize(width_ * height_);
        
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
        initialized_ = true;
        return true;
    }
    
    void shutdown() override {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        initialized_ = false;
    }
    
    void present() override {
        // Blit framebuffer to window
        SetDIBitsToDevice(
            hdc_, 0, 0, width_, height_,
            0, 0, 0, height_,
            framebuffer_.data(), &bmp_info_, DIB_RGB_COLORS
        );
    }
    
    std::unique_ptr<GPUBuffer> create_buffer(const BufferDesc& desc) override {
        return nullptr;
    }
    
    void update_buffer(GPUBuffer* /*buffer*/, const void* /*data*/, size_t /*size*/) override {}
    void destroy_buffer(GPUBuffer* /*buffer*/) override {}
    
    std::unique_ptr<GPUTexture> create_texture(const TextureDesc& desc) override {
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
        uint32_t idx = y * width_ + x;
        if (depth >= depth_buffer_[idx]) return;
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
        SetTextColor(hdc_, RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
        SetBkMode(hdc_, TRANSPARENT);
        TextOutA(hdc_, x, y, text.c_str(), text.length());
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
    
    // Project 3D point to 2D screen coordinates
    void project(const Vec3& world_pos, const Mat4& view_proj, int& screen_x, int& screen_y, float& depth) {
        Vec3 clip = view_proj * world_pos;
        float w = clip.z; // Use z as w for perspective
        if (w < 0.001f) w = 0.001f;
        
        float ndc_x = clip.x / w;
        float ndc_y = clip.y / w;
        
        screen_x = (int)((ndc_x * 0.5f + 0.5f) * width_);
        screen_y = (int)((-ndc_y * 0.5f + 0.5f) * height_);
        depth = w;
    }
    
    // Draw a 3D triangle from world coordinates
    void draw_triangle_3d(
        const Vec3& v0, const Vec3& v1, const Vec3& v2,
        const Mat4& view_proj, uint32_t color) {
        int x0, y0, x1, y1, x2, y2;
        float z0, z1, z2;
        
        project(v0, view_proj, x0, y0, z0);
        project(v1, view_proj, x1, y1, z1);
        project(v2, view_proj, x2, y2, z2);
        
        draw_triangle_depth(x0, y0, z0, x1, y1, z1, x2, y2, z2, color);
    }
    
    // Draw a 3D mesh
    void draw_mesh(const std::vector<Vec3>& vertices, const std::vector<uint32_t>& indices,
                   const Mat4& view_proj, uint32_t color) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            draw_triangle_3d(
                vertices[indices[i]], vertices[indices[i+1]], vertices[indices[i+2]],
                view_proj, color
            );
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
        for (float x = -10.0f; x <= 10.0f; x += spacing) {
            Vec3 start(x, 0, -10);
            Vec3 end(x, 0, 10);
            int x0, y0, x1, y1;
            float z0, z1;
            project(start, view_proj, x0, y0, z0);
            project(end, view_proj, x1, y1, z1);
            draw_line_3d(x0, y0, z0, x1, y1, z1, color);
        }
        for (float z = -10.0f; z <= 10.0f; z += spacing) {
            Vec3 start(-10, 0, z);
            Vec3 end(10, 0, z);
            int x0, y0, x1, y1;
            float z0, z1;
            project(start, view_proj, x0, y0, z0);
            project(end, view_proj, x1, y1, z1);
            draw_line_3d(x0, y0, z0, x1, y1, z1, color);
        }
    }
    
    // Draw a heightmap terrain
    void draw_terrain(const std::vector<float>& heightmap, uint32_t size, 
                      float scale, const Mat4& view_proj, uint32_t color) {
        for (uint32_t z = 0; z < size - 1; z++) {
            for (uint32_t x = 0; x < size - 1; x++) {
                float h00 = heightmap[z * size + x];
                float h10 = heightmap[z * size + (x + 1)];
                float h01 = heightmap[(z + 1) * size + x];
                float h11 = heightmap[(z + 1) * size + (x + 1)];
                
                Vec3 v00((x - size/2) * scale, h00 * scale, (z - size/2) * scale);
                Vec3 v10((x + 1 - size/2) * scale, h10 * scale, (z - size/2) * scale);
                Vec3 v01((x - size/2) * scale, h01 * scale, (z + 1 - size/2) * scale);
                Vec3 v11((x + 1 - size/2) * scale, h11 * scale, (z + 1 - size/2) * scale);
                
                draw_triangle_3d(v00, v10, v01, view_proj, color);
                draw_triangle_3d(v10, v11, v01, view_proj, color);
            }
        }
    }
    
    HWND get_window() const { return hwnd_; }
    
private:
    class SoftwareTexture : public GPUTexture {
    public:
        SoftwareTexture(const TextureDesc& desc) : desc_(desc) {
            pixels_.resize(desc.width * desc.height * 4);
        }
        
        void update(const void* data, uint32_t width, uint32_t height) override {
            memcpy(pixels_.data(), data, width * height * 4);
        }
        
        uint32_t get_width() const override { return desc_.width; }
        uint32_t get_height() const override { return desc_.height; }
        TextureFormat get_format() const override { return desc_.format; }
        
        std::vector<uint8_t> pixels_;
        TextureDesc desc_;
    };
    
    HWND hwnd_ = nullptr;
    HDC hdc_ = nullptr;
    std::vector<uint8_t> framebuffer_;
    std::vector<float> depth_buffer_;
    BITMAPINFO bmp_info_ = {};
    uint32_t width_ = 800;
    uint32_t height_ = 600;
    bool initialized_ = false;
};

} // namespace litt
