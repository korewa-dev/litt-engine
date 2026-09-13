// Rasterizer implementation - Gouraud shading with PBR lighting
#include "litt_rasterizer.h"
#include "litt_lighting.h"
#include <cmath>
#include <algorithm>

namespace litt {

uint32_t LightManager::add_light(const Light& light) {
    lights_.push_back(std::make_unique<Light>(light));
    return next_id_++;
}

void LightManager::remove_light(uint32_t id) {
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(),
        [id](const std::unique_ptr<Light>& l) { return (uint32_t)(uintptr_t)l.get() == id; }), lights_.end());
}

Light* LightManager::get_light(uint32_t id) {
    for (auto& l : lights_) if ((uint32_t)(uintptr_t)l.get() == id) return l.get();
    return nullptr;
}

std::vector<Light*> LightManager::get_lights_affecting_point(const Vec3& point, float radius) const {
    std::vector<Light*> result;
    for (const auto& l : lights_) {
        if (l->type == LightType::DIRECTIONAL) result.push_back(l.get());
        else if ((l->position - point).length() < radius + l->range) result.push_back(l.get());
    }
    return result;
}

void LightManager::clear() { lights_.clear(); }

float PBRLighting::distribution_ggx(const Vec3& normal, const Vec3& half_vec, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = std::max(dot(normal, half_vec), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (MATH_PI * denom * denom + 0.0001f);
}

float PBRLighting::geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float PBRLighting::geometry_smith(const Vec3& normal, const Vec3& view_dir, const Vec3& light_dir, float roughness) {
    float NdotV = std::max(dot(normal, view_dir), 0.0f);
    float NdotL = std::max(dot(normal, light_dir), 0.0f);
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

Vec3 PBRLighting::fresnel_schlick(float cos_theta, Vec3 F0) {
    float t = std::pow(std::max(1.0f - cos_theta, 0.0f), 5.0f);
    return F0 + (Vec3(1.0f, 1.0f, 1.0f) - F0) * t;
}

Vec3 PBRLighting::calculate_direct_light(const Light& light, const Vec3& world_pos,
                                        const Vec3& normal, const Vec3& view_dir,
                                        const Vec3& albedo, float metallic, float roughness) {
    Vec3 light_dir;
    float attenuation = 1.0f;
    if (light.type == LightType::DIRECTIONAL) {
        light_dir = light.direction.normalized();
    } else {
        Vec3 to_light = light.position - world_pos;
        float dist = to_light.length();
        light_dir = to_light / dist;
        attenuation = std::max(0.0f, 1.0f - dist / light.range);
        attenuation *= attenuation;
    }
    Vec3 half_vec = (light_dir + view_dir).normalized();
    float NdotL = std::max(dot(normal, light_dir), 0.0f);
    if (NdotL <= 0.0f) return Vec3::zero();
    Vec3 F0(0.04f, 0.04f, 0.04f);
    F0 = Vec3(F0.x + (albedo.x - F0.x) * metallic, F0.y + (albedo.y - F0.y) * metallic, F0.z + (albedo.z - F0.z) * metallic);
    float D = distribution_ggx(normal, half_vec, roughness);
    float G = geometry_smith(normal, view_dir, light_dir, roughness);
    Vec3 F = fresnel_schlick(std::max(dot(half_vec, view_dir), 0.0f), F0);
    Vec3 specular(D * G * F.x, D * G * F.y, D * G * F.z);
    Vec3 kD = (Vec3(1.0f, 1.0f, 1.0f) - F) * (1.0f - metallic);
    Vec3 diffuse = Vec3(albedo.x * kD.x, albedo.y * kD.y, albedo.z * kD.z) / MATH_PI;
    return (diffuse + specular) * Vec3(light.color.x, light.color.y, light.color.z) * light.intensity * attenuation * NdotL;
}

Vec3 PBRLighting::calculate_irradiance(const Vec3&, const Vec3&) { return Vec3(0.1f, 0.1f, 0.12f); }
Vec3 PBRLighting::ibl_specular(const Vec3&, float, const Vec3&) { return Vec3::zero(); }
Vec3 PBRLighting::ibl_diffuse(const Vec3&, const Vec3&) { return Vec3::zero(); }

Vec3 PBRLighting::fresnel_schlick_roughness(float cos_theta, Vec3 F0, float roughness) {
    float t = std::pow(std::max(1.0f - cos_theta, 0.0f), 5.0f);
    return F0 + (Vec3(std::max(1.0f - roughness, F0.x), std::max(1.0f - roughness, F0.y), std::max(1.0f - roughness, F0.z)) - F0) * t;
}

ShadowMap::ShadowMap(uint32_t size) : size_(size), light_view_proj_(Mat4::identity()) { depth_data_.resize(size * size); }
ShadowMap::~ShadowMap() = default;
void ShadowMap::begin_pass(const Vec3&, const Vec3&, float) { std::fill(depth_data_.begin(), depth_data_.end(), 1.0f); }
void ShadowMap::end_pass() {}
float ShadowMap::sample(float x, float y) const {
    int ix = (int)(x * size_), iy = (int)(y * size_);
    if (ix < 0 || ix >= (int)size_ || iy < 0 || iy >= (int)size_) return 1.0f;
    return depth_data_[iy * size_ + ix];
}

Rasterizer::Rasterizer(const RasterState& state) : state_(state) {}

void Rasterizer::clear(const Vec3& color) {
    pixels_.resize(viewport_width_ * viewport_height_);
    depth_buffer_.resize(viewport_width_ * viewport_height_);
    std::fill(pixels_.begin(), pixels_.end(), color);
    std::fill(depth_buffer_.begin(), depth_buffer_.end(), 1.0f);
}

void Rasterizer::set_viewport(int x, int y, int width, int height) {
    viewport_x_ = x; viewport_y_ = y; viewport_width_ = width; viewport_height_ = height;
}

void Rasterizer::get_viewport(int& x, int& y, int& width, int& height) const {
    x = viewport_x_; y = viewport_y_; width = viewport_width_; height = viewport_height_;
}

void Rasterizer::render_triangle(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                 const Vec3& n1, const Vec3& n2, const Vec3& n3,
                                 const Vec3& color) {
    (void)n1; (void)n2; (void)n3;
    PBRMaterial mat;
    mat.albedo = color;
    render_triangle_pbr(v1, v2, v3, n1, n2, n3, mat);
}

void Rasterizer::render_triangle_pbr(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                     const Vec3& n1, const Vec3& n2, const Vec3& n3,
                                     const PBRMaterial& material) {
    Vec3 view_dir(0, 0, 1);
    auto& lm = LightManager::get_instance();
    Vec3 colors[3];
    Vec3 verts[3] = {v1, v2, v3};
    Vec3 normals[3] = {n1.normalized(), n2.normalized(), n3.normalized()};
    for (int i = 0; i < 3; i++) {
        Vec3 total_light(material.emission.x, material.emission.y, material.emission.z);
        total_light = total_light + PBRLighting::calculate_irradiance(normals[i], Vec3());
        auto lights = lm.get_lights_affecting_point(verts[i], 50.0f);
        for (const Light* light : lights) {
            total_light = total_light + PBRLighting::calculate_direct_light(
                *light, verts[i], normals[i], view_dir, material.albedo, material.metallic, material.roughness);
        }
        colors[i] = total_light * material.ao;
    }
    // Rasterize with interpolated colors
    int min_x, max_x, min_y, max_y;
    compute_triangle_bounds(v1, v2, v3, min_x, max_x, min_y, max_y);
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            Vec3 p(x, y, 0);
            if (!point_in_triangle(p, v1, v2, v3)) continue;
            float w1 = ((v2.y - v3.y) * (p.x - v3.x) + (v3.x - v2.x) * (p.y - v3.y)) /
                       ((v2.y - v3.y) * (v1.x - v3.x) + (v3.x - v2.x) * (v1.y - v3.y));
            float w2 = ((v3.y - v1.y) * (p.x - v3.x) + (v1.x - v3.x) * (p.y - v3.y)) /
                       ((v2.y - v3.y) * (v1.x - v3.x) + (v3.x - v2.x) * (v1.y - v3.y));
            float w3 = 1.0f - w1 - w2;
            float depth = w1 * v1.z + w2 * v2.z + w3 * v3.z;
            if (!depth_test_pass(depth, x, y)) continue;
            write_depth(x, y, depth);
            Vec3 color = colors[0] * w1 + colors[1] * w2 + colors[2] * w3;
            pixels_[y * viewport_width_ + x] = Vec3(std::min(color.x, 1.0f), std::min(color.y, 1.0f), std::min(color.z, 1.0f));
        }
    }
}

bool Rasterizer::depth_test_pass(float new_depth, int x, int y) const { return new_depth < depth_buffer_[y * viewport_width_ + x]; }
void Rasterizer::write_depth(int x, int y, float depth) { depth_buffer_[y * viewport_width_ + x] = depth; }

bool Rasterizer::point_in_triangle(const Vec3& P, const Vec3& A, const Vec3& B, const Vec3& C) const {
    auto sign = [](const Vec3& p1, const Vec3& p2, const Vec3& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    float d1 = sign(P, A, B), d2 = sign(P, B, C), d3 = sign(P, C, A);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

void Rasterizer::compute_triangle_bounds(const Vec3& v1, const Vec3& v2, const Vec3& v3,
                                        int& min_x, int& max_x, int& min_y, int& max_y) const {
    min_x = std::max((int)std::min({v1.x, v2.x, v3.x}), 0);
    max_x = std::min((int)std::max({v1.x, v2.x, v3.x}), viewport_width_ - 1);
    min_y = std::max((int)std::min({v1.y, v2.y, v3.y}), 0);
    max_y = std::min((int)std::max({v1.y, v2.y, v3.y}), viewport_height_ - 1);
}

Vec3 Rasterizer::sample_albedo(const PBRMaterial& material) const { return material.albedo; }
Vec3 Rasterizer::sample_normal(const PBRMaterial&) const { return Vec3(0, 0, 1); }
float Rasterizer::sample_metallic(const PBRMaterial& material) const { return material.metallic; }
float Rasterizer::sample_roughness(const PBRMaterial& material) const { return material.roughness; }
float Rasterizer::sample_ao(const PBRMaterial& material) const { return material.ao; }
Vec3 Rasterizer::sample_emission(const PBRMaterial& material) const { return material.emission; }

FrameBuffer::FrameBuffer(int width, int height) : width_(width), height_(height) {
    color_attachments_.push_back(std::vector<Vec3>(width * height));
    depth_buffer_.resize(width * height);
}
void FrameBuffer::bind() { is_bound_ = true; }
void FrameBuffer::unbind() { is_bound_ = false; }
void FrameBuffer::resize(int width, int height) {
    width_ = width; height_ = height;
    for (auto& ca : color_attachments_) ca.resize(width * height);
    depth_buffer_.resize(width * height);
}
void FrameBuffer::add_color_attachment() { color_attachments_.push_back(std::vector<Vec3>(width_ * height_)); }
void FrameBuffer::add_depth_attachment() { depth_buffer_.resize(width_ * height_); }

void SSR::apply_screen_space_reflections(const std::vector<Vec3>& scene_pixels,
                                         const std::vector<float>& depth_buffer, const RasterState&) {
    current_scene_ = scene_pixels; current_depth_ = depth_buffer;
}

void RasterUtils::rasterize_line(int x0, int y0, int x1, int y1, std::vector<Vec3>& pixels, const Vec3& color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (x0 >= 0 && x0 < 800 && y0 >= 0 && y0 < 600) pixels[y0 * 800 + x0] = color;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void RasterUtils::rasterize_circle(int cx, int cy, int r, std::vector<Vec3>& pixels, const Vec3& color) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) {
        if (x*x + y*y <= r*r) {
            int px = cx + x, py = cy + y;
            if (px >= 0 && px < 800 && py >= 0 && py < 600) pixels[py * 800 + px] = color;
        }
    }
}

void RasterUtils::supersample_triangle(const Vec3&, const Vec3&, const Vec3&, std::vector<Vec3>&, const Vec3&, int) {}
Vec3 RasterUtils::world_to_screen(const Vec3&, const float*, const float*, const float*, int, int, int, int) { return Vec3::zero(); }
Vec3 RasterUtils::screen_to_world(const Vec3&, const float*, const float*, const float*) { return Vec3::zero(); }

} // namespace litt
