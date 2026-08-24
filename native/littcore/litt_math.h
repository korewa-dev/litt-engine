// LittMath - Production C++17 math for game engines
// SIMD-ready, no dependencies, template-based

#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>

#ifndef MATH_PI
#define MATH_PI 3.14159265358979323846f
#endif
#ifndef MATH_EPS
#define MATH_EPS 1e-6f
#endif

namespace litt {

// ================================================================
// Vec2
// ================================================================
struct alignas(8) Vec2 {
    float x, y;
    constexpr Vec2() : x(0), y(0) {}
    constexpr Vec2(float x, float y) : x(x), y(y) {}
    static constexpr Vec2 zero() { return {0, 0}; }
    static constexpr Vec2 one() { return {1, 1}; }
    static constexpr Vec2 unit_x() { return {1, 0}; }
    static constexpr Vec2 unit_y() { return {0, 1}; }
    
    float length_sq() const { return x*x + y*y; }
    float length() const { return std::sqrt(length_sq()); }
    Vec2 normalized() const {
        float l = length();
        return l > MATH_EPS ? Vec2(x/l, y/l) : zero();
    }
    float dot(const Vec2& b) const { return x*b.x + y*b.y; }
    float cross(const Vec2& b) const { return x*b.y - y*b.x; }
    
    Vec2 operator+(const Vec2& b) const { return {x+b.x, y+b.y}; }
    Vec2 operator-(const Vec2& b) const { return {x-b.x, y-b.y}; }
    Vec2 operator*(float s) const { return {x*s, y*s}; }
    Vec2 operator/(float s) const { float i = 1.0f/s; return {x*i, y*i}; }
    Vec2 operator-() const { return {-x, -y}; }
    
    Vec2& operator+=(const Vec2& b) { x += b.x; y += b.y; return *this; }
    Vec2& operator-=(const Vec2& b) { x -= b.x; y -= b.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    
    bool operator==(const Vec2& b) const { return x == b.x && y == b.y; }
    bool operator!=(const Vec2& b) const { return !(*this == b); }
    
    Vec2 lerp(const Vec2& b, float t) const {
        return {x + (b.x - x) * t, y + (b.y - y) * t};
    }
    Vec2 clamp(float mn, float mx) const {
        return {std::clamp(x, mn, mx), std::clamp(y, mn, mx)};
    }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

// ================================================================
// Vec3
// ================================================================
struct alignas(16) Vec3 {
    float x, y, z;
    constexpr Vec3() : x(0), y(0), z(0) {}
    constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    static constexpr Vec3 zero() { return {0, 0, 0}; }
    static constexpr Vec3 one() { return {1, 1, 1}; }
    static constexpr Vec3 unit_x() { return {1, 0, 0}; }
    static constexpr Vec3 unit_y() { return {0, 1, 0}; }
    static constexpr Vec3 unit_z() { return {0, 0, 1}; }
    static constexpr Vec3 up() { return unit_y(); }
    static constexpr Vec3 right() { return unit_x(); }
    static constexpr Vec3 forward() { return unit_z(); }
    
    float length_sq() const { return x*x + y*y + z*z; }
    float length() const { return std::sqrt(length_sq()); }
    Vec3 normalized() const {
        float l = length();
        return l > MATH_EPS ? Vec3(x/l, y/l, z/l) : zero();
    }
    float dot(const Vec3& b) const { return x*b.x + y*b.y + z*b.z; }
    Vec3 cross(const Vec3& b) const {
        return {y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x};
    }
    
    Vec3 operator+(const Vec3& b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(const Vec3& b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(float s) const { float i = 1.0f/s; return {x*i, y*i, z*i}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    
    Vec3& operator+=(const Vec3& b) { x += b.x; y += b.y; z += b.z; return *this; }
    Vec3& operator-=(const Vec3& b) { x -= b.x; y -= b.y; z -= b.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    
    bool operator==(const Vec3& b) const { return x == b.x && y == b.y && z == b.z; }
    bool operator!=(const Vec3& b) const { return !(*this == b); }

    float operator[](int i) const { return (&x)[i]; }
    Vec3 reflect(const Vec3& n) const {
        return *this - n * (dot(n) * 2.0f);
    }
    
    // Refract through normal
    Vec3 refract(const Vec3& n, float eta) const {
        float n_dot_v = dot(n);
        Vec3 v_perp = *this - n * n_dot_v;
        float disc = 1.0f - eta * eta * (1.0f - n_dot_v * n_dot_v);
        if (disc < 0) return zero();
        return v_perp * eta - n * std::sqrt(disc);
    }
    
    Vec3 lerp(const Vec3& b, float t) const {
        return {x + (b.x - x) * t, y + (b.y - y) * t, z + (b.z - z) * t};
    }
    
    Vec3 slerp(const Vec3& b, float t) const {
        // For unit vectors
        float cos_a = dot(b);
        cos_a = std::clamp(cos_a, -1.0f, 1.0f);
        float angle = std::acos(cos_a);
        if (angle < MATH_EPS) return normalized();
        float sin_a = std::sin(angle);
        float s0 = std::sin((1.0f - t) * angle) / sin_a;
        float s1 = std::sin(t * angle) / sin_a;
        return *this * s0 + b * s1;
    }
    
    Vec3 clamp(float mn, float mx) const {
        return {std::clamp(x, mn, mx), std::clamp(y, mn, mx), std::clamp(z, mn, mx)};
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

// ================================================================
// Vec4
// ================================================================
struct alignas(16) Vec4 {
    float x, y, z, w;
    constexpr Vec4() : x(0), y(0), z(0), w(0) {}
    constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr explicit Vec4(float s) : x(s), y(s), z(s), w(s) {}
    static constexpr Vec4 zero() { return {0, 0, 0, 0}; }
    static constexpr Vec4 one() { return {1, 1, 1, 1}; }
    
    float dot(const Vec4& b) const { return x*b.x + y*b.y + z*b.z + w*b.w; }
    Vec3 xyz() const { return {x, y, z}; }
    
    Vec4 operator+(const Vec4& b) const { return {x+b.x, y+b.y, z+b.z, w+b.w}; }
    Vec4 operator-(const Vec4& b) const { return {x-b.x, y-b.y, z-b.z, w-b.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
    Vec4 operator/(float s) const { float i = 1.0f/s; return {x*i, y*i, z*i, w*i}; }
    Vec4 operator-() const { return {-x, -y, -z, -w}; }
    
    bool operator==(const Vec4& b) const { return x == b.x && y == b.y && z == b.z && w == b.w; }
    bool operator!=(const Vec4& b) const { return !(*this == b); }
};

// ================================================================
// Mat4 (column-major, OpenGL convention)
// ================================================================
struct alignas(16) Mat4 {
    float m[16];
    
    Mat4() { std::memset(m, 0, sizeof(m)); }
    explicit Mat4(float diag) { *this = identity(); m[0] = m[5] = m[10] = m[15] = diag; }
    
    static Mat4 identity() {
        Mat4 r; std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    
    static Mat4 perspective(float fov_deg, float aspect, float near, float far) {
        Mat4 r; std::memset(r.m, 0, sizeof(r.m));
        float f = 1.0f / std::tan(fov_deg * 0.5f * MATH_PI / 180.0f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = -(far + near) / (far - near);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * far * near) / (far - near);
        return r;
    }
    
    static Mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
        Mat4 r; std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (far - near);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(far + near) / (far - near);
        r.m[15] = 1.0f;
        return r;
    }
    
    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 z = (target - eye).normalized();
        Vec3 x = up.cross(z).normalized();
        Vec3 y = z.cross(x);
        Mat4 r;
        // View matrix: camera axes stored as ROWS (world -> camera transform)
        r.m[0] = x.x; r.m[4] = x.y; r.m[8]  = x.z;
        r.m[1] = y.x; r.m[5] = y.y; r.m[9]  = y.z;
        r.m[2] = z.x; r.m[6] = z.y; r.m[10] = z.z;
        r.m[12] = -x.dot(eye); r.m[13] = -y.dot(eye); r.m[14] = -z.dot(eye);
        r.m[3] = r.m[7] = r.m[11] = 0; r.m[15] = 1;
        return r;
    }
    
    static Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    
    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }
    
    static Mat4 rot_x(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        return r;
    }
    
    static Mat4 rot_y(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        return r;
    }
    
    static Mat4 rot_z(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
        return r;
    }
    
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                float s = 0;
                for (int k = 0; k < 4; k++)
                    s += m[k + j*4] * b.m[i + k*4];
                r.m[i + j*4] = s;
            }
        return r;
    }
    
    Vec3 operator*(const Vec3& v) const {
        return {
            m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12],
            m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13],
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]
        };
    }
    
    Vec4 operator*(const Vec4& v) const {
        return {
            m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        };
    }
    
    Mat4 operator+(const Mat4& b) const {
        Mat4 r;
        for (int i = 0; i < 16; i++) r.m[i] = m[i] + b.m[i];
        return r;
    }
    
    Mat4 operator*(float s) const {
        Mat4 r;
        for (int i = 0; i < 16; i++) r.m[i] = m[i] * s;
        return r;
    }
    
    Mat4& operator*=(float s) {
        for (int i = 0; i < 16; i++) m[i] *= s;
        return *this;
    }
    
    Mat4 transpose() const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[j*4+i] = m[i*4+j];
        return r;
    }
    
    // Fast inverse for affine transforms (no perspective)
    Mat4 affine_inverse() const {
        Mat4 r = identity();
        // Transpose upper 3x3
        r.m[0] = m[0]; r.m[1] = m[4]; r.m[2] = m[8];
        r.m[4] = m[1]; r.m[5] = m[5]; r.m[6] = m[9];
        r.m[8] = m[2]; r.m[9] = m[6]; r.m[10] = m[10];
        // Negate translation
        r.m[12] = -(m[0]*m[12] + m[4]*m[13] + m[8]*m[14]);
        r.m[13] = -(m[1]*m[12] + m[5]*m[13] + m[9]*m[14]);
        r.m[14] = -(m[2]*m[12] + m[6]*m[13] + m[10]*m[14]);
        return r;
    }
};

// ================================================================
// Quat
// ================================================================
struct alignas(16) Quat {
    float x, y, z, w;
    constexpr Quat() : x(0), y(0), z(0), w(1) {}
    constexpr Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    static constexpr Quat identity() { return {0, 0, 0, 1}; }
    
    static Quat from_axis_angle(const Vec3& axis, float angle) {
        Vec3 n = axis.normalized();
        float c = std::cos(angle * 0.5f);
        float s = std::sin(angle * 0.5f);
        return {n.x*s, n.y*s, n.z*s, c};
    }
    
    static Quat from_euler(const Vec3& euler) {
        float cx = std::cos(euler.x * 0.5f), sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f), sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f), sz = std::sin(euler.z * 0.5f);
        return {
            sx*cy*cz - cx*sy*sz,
            cx*sy*cz + sx*cy*sz,
            cx*cy*sz - sx*sy*cz,
            cx*cy*cz + sx*sy*sz
        };
    }
    
    Quat operator*(const Quat& b) const {
        return {
            w*b.x + x*b.w + y*b.z - z*b.y,
            w*b.y - x*b.z + y*b.w + z*b.x,
            w*b.z + x*b.y - y*b.x + z*b.w,
            w*b.w - x*b.x - y*b.y - z*b.z
        };
    }
    
    Quat normalized() const {
        float l = std::sqrt(x*x + y*y + z*z + w*w);
        return l > MATH_EPS ? Quat(x/l, y/l, z/l, w/l) : identity();
    }
    
    Quat conjugate() const { return {-x, -y, -z, w}; }
    
    // Transform vector
    Vec3 transform(const Vec3& v) const {
        Quat qv = {v.x, v.y, v.z, 0};
        Quat res = (*this) * qv * conjugate();
        return {res.x, res.y, res.z};
    }
    
    // To matrix
    Mat4 to_mat4() const {
        Quat n = normalized();
        float xx = n.x*n.x, yy = n.y*n.y, zz = n.z*n.z;
        float xy = n.x*n.y, xz = n.x*n.z, yz = n.y*n.z;
        float wx = n.w*n.x, wy = n.w*n.y, wz = n.w*n.z;
        Mat4 r;
        r.m[0] = 1-2*(yy+zz); r.m[1] = 2*(xy+wz); r.m[2] = 2*(xz-wy);
        r.m[4] = 2*(xy-wz); r.m[5] = 1-2*(xx+zz); r.m[6] = 2*(yz+wx);
        r.m[8] = 2*(xz+wy); r.m[9] = 2*(yz-wx); r.m[10] = 1-2*(xx+yy);
        r.m[3] = r.m[7] = r.m[11] = 0; r.m[12] = r.m[13] = r.m[14] = 0; r.m[15] = 1;
        return r;
    }
    
    // To euler
    Vec3 to_euler() const {
        Quat n = normalized();
        float sy = 2*(n.w*n.y - n.z*n.x);
        float cx = 1-2*(n.y*n.y + n.z*n.z);
        float cy = 1-2*(n.x*n.x + n.z*n.z);
        float cz = 1-2*(n.x*n.x + n.y*n.y);
        float sx = 2*(n.w*n.x + n.y*n.z);
        float sz = 2*(n.w*n.z + n.x*n.y);
        return {std::atan2(sx, cx), std::asin(std::clamp(sy, -1.0f, 1.0f)), std::atan2(sz, cz)};
    }
};

// ================================================================
// AABB
// ================================================================
struct Aabb {
    Vec3 min, max;
    constexpr Aabb() : min(0,0,0), max(0,0,0) {}
    constexpr Aabb(const Vec3& mn, const Vec3& mx) : min(mn), max(mx) {}
    static Aabb empty() { return {Vec3(1e10f, 1e10f, 1e10f), Vec3(-1e10f, -1e10f, -1e10f)}; }
    static Aabb infinite() { return {Vec3(-1e10f, -1e10f, -1e10f), Vec3(1e10f, 1e10f, 1e10f)}; }
    
    bool contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }
    bool intersects(const Aabb& b) const {
        return max.x >= b.min.x && min.x <= b.max.x && max.y >= b.min.y && min.y <= b.max.y && max.z >= b.min.z && min.z <= b.max.z;
    }
    bool intersects_sphere(const Vec3& center, float radius) const {
        Vec3 c = {std::clamp(center.x, min.x, max.x), std::clamp(center.y, min.y, max.y), std::clamp(center.z, min.z, max.z)};
        return (center - c).length_sq() <= radius * radius;
    }
    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 size() const { return max - min; }
    float radius() const { return size().length() * 0.5f; }
    float volume() const { Vec3 s = size(); return s.x * s.y * s.z; }
    Aabb merge(const Aabb& b) const {
        return {
            Vec3(std::min(min.x, b.min.x), std::min(min.y, b.min.y), std::min(min.z, b.min.z)),
            Vec3(std::max(max.x, b.max.x), std::max(max.y, b.max.y), std::max(max.z, b.max.z))
        };
    }
    Aabb expand(const Vec3& p) const {
        return {
            Vec3(std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z)),
            Vec3(std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z))
        };
    }
};

// ================================================================
// Ray
// ================================================================
struct Ray {
    Vec3 origin, direction;
    float t_min, t_max;
    constexpr Ray() : origin(0,0,0), direction(0,0,1), t_min(0), t_max(1e10f) {}
    constexpr Ray(const Vec3& o, const Vec3& d, float near = 0, float far = 1e10f)
        : origin(o), direction(d.normalized()), t_min(near), t_max(far) {}
    Vec3 at(float t) const { return origin + direction * t; }
};

// ================================================================
// Hit Info
// ================================================================
struct HitInfo {
    bool hit = false;
    float t = 1e10f;
    Vec3 point, normal;
    void* material = nullptr;
};

// ================================================================
// Utility Functions
// ================================================================
inline float deg_to_rad(float d) { return d * MATH_PI / 180.0f; }
inline float rad_to_deg(float r) { return r * 180.0f / MATH_PI; }
inline float clamp(float v, float mn, float mx) { return std::max(mn, std::min(mx, v)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float distance(const Vec2& a, const Vec2& b) { return (b - a).length(); }
inline float distance(const Vec3& a, const Vec3& b) { return (b - a).length(); }
inline float dot(const Vec2& a, const Vec2& b) { return a.dot(b); }
inline float dot(const Vec3& a, const Vec3& b) { return a.dot(b); }
inline Vec3 cross(const Vec3& a, const Vec3& b) { return a.cross(b); }
inline Vec3 normalize(const Vec3& v) { return v.normalized(); }
inline Vec2 normalize(const Vec2& v) { return v.normalized(); }

// Triangle normal
inline Vec3 triangle_normal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).cross(c - a).normalized();
}

// Barycentric coords
inline Vec3 barycentric(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = v0.dot(v0), d01 = v0.dot(v1), d11 = v1.dot(v1);
    float d20 = v2.dot(v0), d21 = v2.dot(v1);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    return {1 - v - w, v, w};
}

// Ray-AABB intersection
inline HitInfo ray_aabb(const Ray& r, const Aabb& a) {
    float tmin = -1e10f, tmax = 1e10f;
    for (int i = 0; i < 3; i++) {
        float o = r.origin[i], d = r.direction[i];
        float mn = a.min[i], mx = a.max[i];
        if (std::abs(d) < 1e-8f) {
            if (o < mn || o > mx) return {};
        } else {
            float t1 = (mn - o) / d, t2 = (mx - o) / d;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return {};
        }
    }
    float t = tmin > r.t_min ? tmin : r.t_max;
    if (t < r.t_min || t > r.t_max) return {};
    Vec3 p = r.at(t);
    Vec3 n;
    if (std::abs(p.x - a.min.x) < 1e-5f) n = Vec3{-1,0,0};
    else if (std::abs(p.x - a.max.x) < 1e-5f) n = Vec3{1,0,0};
    else if (std::abs(p.y - a.min.y) < 1e-5f) n = Vec3{0,-1,0};
    else if (std::abs(p.y - a.max.y) < 1e-5f) n = Vec3{0,1,0};
    else if (std::abs(p.z - a.min.z) < 1e-5f) n = Vec3{0,0,-1};
    else n = Vec3{0,0,1};
    return {true, t, p, n};
}

// Ray-triangle intersection (Möller-Trumbore)
inline HitInfo ray_triangle(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2) {
    Vec3 e1 = v1 - v0, e2 = v2 - v0;
    Vec3 h = r.direction.cross(e2);
    float a = e1.dot(h);
    if (a > -1e-8f && a < 1e-8f) return {};
    float f = 1.0f / a;
    Vec3 s = r.origin - v0;
    float u = f * s.dot(h);
    if (u < 0 || u > 1) return {};
    Vec3 q = s.cross(e1);
    float v = f * r.direction.dot(q);
    if (v < 0 || u + v > 1) return {};
    float t = f * e2.dot(q);
    if (t > r.t_min && t < r.t_max) {
        return {true, t, r.at(t), (v1-v0).cross(v2-v0).normalized()};
    }
    return {};
}

} // namespace litt
