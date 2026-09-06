// Windows Software Renderer - Real framebuffer output using GDI
// No external dependencies - uses Windows built-in GDI for pixel output

#pragma once
#include "litt_gpu.h"
#include <windows.h>
#include <vector>
#include <cstring>
#include <iostream>

namespace litt {

class SoftwareRenderer : public IGPUDevice {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override { shutdown(); }
    
    bool initialize(const std::string& /*adapter_name*/ = "") override {
        if (initialized_) return true;
        
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
        // Software renderer doesn't use GPU buffers
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
    
    // Software rendering API
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
        // Simple text rendering using GDI
        SetTextColor(hdc_, RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
        SetBkMode(hdc_, TRANSPARENT);
        TextOutA(hdc_, x, y, text.c_str(), text.length());
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
