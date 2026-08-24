// LittMath Implementation - Core math operations
// Implements the inline functions from litt_math.h

#include "litt_math.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

// Explicit template instantiations for common types
template struct litt::Vec2<float>;
template struct litt::Vec2<double>;
template struct litt::Vec3<float>;
template struct litt::Vec3<double>;
template struct litt::Vec4<float>;
template struct litt::Vec4<double>;
template struct litt::Mat4<float>;
template struct litt::Mat4<double>;
template struct litt::Quat<float>;
template struct litt::Quat<double>;
template struct litt::Aabb<float>;
template struct litt::Aabb<double>;
template struct litt::Plane<float>;
template struct litt::Plane<double>;
template struct litt::Ray<float>;
template struct litt::Ray<double>;
template struct litt::HitInfo<float>;
template struct litt::HitInfo<double>;

namespace litt {

// =============================================================================
// Vec3*Mat4 operator (defined outside struct to avoid header issues)
// =============================================================================
template<typename T>
Vec3<T> Vec3<T>::operator*(const Mat4<T>& m) const {
    T w = m.m[3] * x + m.m[7] * y + m.m[11] * z + m.m[15];
    return Vec3<T>(
        (m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12]) / w,
        (m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13]) / w,
        (m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14]) / w
    );
}

// =============================================================================
// Utility Functions
// =============================================================================

// IO
std::ostream& operator<<(std::ostream& os, const Vec2f& v) {
    return os << "(" << v.x << ", " << v.y << ")";
}

std::ostream& operator<<(std::ostream& os, const Vec3f& v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

std::ostream& operator<<(std::ostream& os, const Vec4f& v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
}

std::ostream& operator<<(std::ostream& os, const Mat4f& m) {
    os << std::fixed << std::setprecision(4);
    for (int i = 0; i < 4; i++) {
        os << "[" << m.m[i*4+0] << ", " << m.m[i*4+1] << ", " 
           << m.m[i*4+2] << ", " << m.m[i*4+3] << "]\n";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Quatf& q) {
    return os << "(" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << ")";
}

std::ostream& operator<<(std::ostream& os, const Aabbf& a) {
    return os << "min:" << a.min << " max:" << a.max;
}

// =============================================================================
// Random Number Generation
// =============================================================================
namespace rng {

// PCG Random (Portable, Compact, Fast)
class PCGRng {
public:
    using state_type = uint64_t;
    
    PCGRng() : state_(1234567890uLL), incr_(6364136223846793005uLL) {}
    PCGRng(uint64_t seed) : state_(seed), incr_(6364136223846793005uLL) {}
    PCGRng(uint64_t seed, uint64_t seq) : state_(seed), incr_(seq * 2 + 1) {}
    
    // 32-bit unsigned
    uint32_t next_u32() {
        state_ = state_ * 6364136223846793005uLL + incr_;
        uint32_t result = (state_ >> 33) ^ state_;
        return result;
    }
    
    // Range [0, max)
    uint32_t next_u32(uint32_t max) {
        uint32_t threshold = ((uint64_t)0x100000000u - max) % max;
        uint32_t r;
        while (true) {
            r = next_u32();
            if (r >= threshold) break;
        }
        return r;
    }
    
    // Range [min, max)
    uint32_t next_u32(uint32_t min, uint32_t max) {
        return min + next_u32(max - min);
    }
    
    // Float [0, 1)
    float next_f32() {
        return next_u32() / float(0xFFFFFFFFu);
    }
    
    // Float in range [min, max)
    float next_f32(float min, float max) {
        return min + next_f32() * (max - min);
    }
    
    // Vec3 random point in unit sphere
    Vec3f random_point_in_sphere() {
        while (true) {
            Vec3f p(next_f32(-1, 1), next_f32(-1, 1), next_f32(-1, 1));
            if (p.length_sq() < 1.0f) return p.normalized();
        }
    }
    
    // Vec3 random point on unit sphere
    Vec3f random_point_on_sphere() {
        float phi = next_f32(0, 2.0f * PI);
        float cos_theta = next_f32(-1, 1);
        float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
        return Vec3f(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
    }
    
    // Deterministic hash
    static uint32_t hash(const void* data, size_t len) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
        uint32_t h = 0x811c9dc5u;
        for (size_t i = 0; i < len; i++) {
            h ^= p[i];
            h *= 0x01000193u;
        }
        return h;
    }
    
private:
    state_type state_;
    state_type incr_;
    static constexpr float PI = 3.14159265358979323846f;
};

// Thread-local RNG instance
inline PCFRng& get_thread_rng() {
    static thread_local PCFRng rng(1234567890uLL);
    return rng;
}

} // namespace rng

// =============================================================================
// Matrix Utilities
// =============================================================================
namespace math {

// Compute normal matrix (transpose of inverse)
template<typename T>
Mat4<T> normal_matrix(const Mat4<T>& m) {
    Mat4<T> inv = m.inverse();
    return inv.transpose();
}

// Compute view matrix from camera position and target
template<typename T>
Mat4<T> view_matrix(const Vec3<T>& eye, const Vec3<T>& target, const Vec3<T>& up) {
    return Mat4<T>::look_at(eye, target, up);
}

// Compute projection matrix
template<typename T>
Mat4<T> projection_matrix(T fov_y_rad, T aspect, T near, T far) {
    return Mat4<T>::perspective(fov_y_rad, aspect, near, far);
}

// Compute orthographic projection
template<typename T>
Mat4<T> ortho_matrix(T left, T right, T bottom, T top, T near, T far) {
    T lr = T(1) / (left - right);
    T bt = T(1) / (bottom - top);
    T nf = T(1) / (near - far);
    
    return Mat4<T>(
        Vec4<T>(T(-2) * lr, T(0), T(0), T(0)),
        Vec4<T>(T(0), T(-2) * bt, T(0), T(0)),
        Vec4<T>(T(0), T(0), T(2) * nf, T(0)),
        Vec4<T>((left + right) * lr, (top + bottom) * bt, (near + far) * nf, T(1))
    );
}

} // namespace math

} // namespace litt
