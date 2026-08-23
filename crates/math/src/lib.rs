//! Ultra-lightweight math types for the path tracer.
//! No external math library -- hand-rolled SIMD-friendly types.
//!
//! Zero-cost abstractions, no heap allocation, no trait objects.

#![cfg_attr(not(feature = "std"), no_std)]
#![allow(clippy::many_single_char_names)]
#![allow(clippy::too_many_arguments)]

#[cfg(feature = "std")]
extern crate std;

use bytemuck::{Pod, Zeroable};
use core::ops::{Add, Sub, Mul, Div, Neg, AddAssign, SubAssign, MulAssign, DivAssign};
use core::mem::MaybeUninit;
use core::ptr;

// =============================================================================
// Vector Types
// =============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Pod, Zeroable)]
#[repr(C)]
pub struct Vec2(pub f32, pub f32);
impl Default for Vec2 {
    fn default() -> Self { Vec2(0.0, 0.0) }
}

impl From<(f32, f32)> for Vec2 {
    fn from(t: (f32, f32)) -> Self { Vec2(t.0, t.1) }
}
impl From<Vec2> for (f32, f32) {
    fn from(v: Vec2) -> Self { (v.0, v.1) }
}


impl Vec2 {
    pub const ZERO: Self = Self(0.0, 0.0);
    pub const ONE: Self = Self(1.0, 1.0);

    #[inline]
    pub const fn new(x: f32, y: f32) -> Self { Self(x, y) }

    #[inline]
    pub fn dot(self, other: Self) -> f32 { self.0 * other.0 + self.1 * other.1 }

    #[inline]
    pub fn length_sq(self) -> f32 { self.dot(self) }

    #[inline]
    pub fn length(self) -> f32 { self.length_sq().sqrt() }

    #[inline]
    pub fn normalized(self) -> Self {
        let len = self.length();
        if len > 0.0 { self * (1.0 / len) } else { Self::ZERO }
    }

    #[inline]
    pub fn lerp(self, other: Self, t: f32) -> Self {
        Self(self.0 + t * (other.0 - self.0), self.1 + t * (other.1 - self.1))
    }
}

impl Add for Vec2 { type Output = Self; fn add(self, other: Self) -> Self { Self(self.0 + other.0, self.1 + other.1) } }
impl Sub for Vec2 { type Output = Self; fn sub(self, other: Self) -> Self { Self(self.0 - other.0, self.1 - other.1) } }
impl Mul<f32> for Vec2 { type Output = Self; fn mul(self, s: f32) -> Self { Self(self.0 * s, self.1 * s) } }
impl Div<f32> for Vec2 { type Output = Self; fn div(self, s: f32) -> Self { Self(self.0 / s, self.1 / s) } }
impl Neg for Vec2 { type Output = Self; fn neg(self) -> Self { Self(-self.0, -self.1) } }
impl AddAssign for Vec2 { fn add_assign(&mut self, other: Self) { self.0 += other.0; self.1 += other.1; } }
impl SubAssign for Vec2 { fn sub_assign(&mut self, other: Self) { self.0 -= other.0; self.1 -= other.1; } }
impl MulAssign<f32> for Vec2 { fn mul_assign(&mut self, s: f32) { self.0 *= s; self.1 *= s; } }

// =============================================================================
// Vec3 - Core type for path tracing
// =============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Default, Pod, Zeroable)]
#[repr(C)]
pub struct Vec3(pub f32, pub f32, pub f32);

impl Vec3 {
    pub const ZERO: Self = Self(0.0, 0.0, 0.0);
    pub const ONE: Self = Self(1.0, 1.0, 1.0);
    pub const X: Self = Self(1.0, 0.0, 0.0);
    pub const Y: Self = Self(0.0, 1.0, 0.0);
    pub const Z: Self = Self(0.0, 0.0, 1.0);

    #[inline]
    pub const fn new(x: f32, y: f32, z: f32) -> Self { Self(x, y, z) }

    #[inline]
    pub fn dot(self, other: Self) -> f32 { self.0 * other.0 + self.1 * other.1 + self.2 * other.2 }

    #[inline]
    pub fn cross(self, other: Self) -> Self {
        Self(
            self.1 * other.2 - self.2 * other.1,
            self.2 * other.0 - self.0 * other.2,
            self.0 * other.1 - self.1 * other.0,
        )
    }

    #[inline]
    pub fn length_sq(self) -> f32 { self.dot(self) }

    #[inline]
    pub fn length(self) -> f32 { self.length_sq().sqrt() }

    #[inline]
    pub fn normalized(self) -> Self {
        let len = self.length();
        if len > 1e-8 { self * (1.0 / len) } else { Self::X }
    }

    #[inline]
    pub fn lerp(self, other: Self, t: f32) -> Self {
        Self(
            self.0 + t * (other.0 - self.0),
            self.1 + t * (other.1 - self.1),
            self.2 + t * (other.2 - self.2),
        )
    }

    #[inline]
    pub fn reflect(self, normal: Self) -> Self {
        self - normal * 2.0 * self.dot(normal)
    }

    #[inline]
    pub fn refract(self, normal: Self, eta: f32) -> Option<Self> {
        let cos = self.dot(normal);
        let k = 1.0 + eta * eta * (cos * cos - 1.0);
        if k < 0.0 { None } else {
            Some(self * eta - normal * (eta * cos + k.sqrt()))
        }
    }

    /// Random point on unit hemisphere around this normal
    #[inline]
    pub fn random_hemisphere(self, u: f32, v: f32) -> Self {
        // Transform to local space, random point on sphere, back to world
        let sign = if self.1 >= 0.0 { 1.0 } else { -1.0 };
        let a = 1.0 / (1.0 + sign * self.1);
        let tangent = Self(sign * self.2 * a, -sign * self.0 * a, -sign * a);
        let bitangent = self.cross(tangent);
        let theta = 2.0 * core::f32::consts::PI * v;
        let r = u.sqrt();
        let phi = theta;
        let sp = r * phi.sin();
        let up = r * phi.cos();
        tangent * sp * phi.sin() + bitangent * sp * phi.cos() + self * up
    }
}

impl Add for Vec3 { type Output = Self; fn add(self, other: Self) -> Self { Self(self.0 + other.0, self.1 + other.1, self.2 + other.2) } }
impl Sub for Vec3 { type Output = Self; fn sub(self, other: Self) -> Self { Self(self.0 - other.0, self.1 - other.1, self.2 - other.2) } }
impl Mul<f32> for Vec3 { type Output = Self; fn mul(self, s: f32) -> Self { Self(self.0 * s, self.1 * s, self.2 * s) } }
impl Div<f32> for Vec3 { type Output = Self; fn div(self, s: f32) -> Self { Self(self.0 / s, self.1 / s, self.2 / s) } }
impl Neg for Vec3 { type Output = Self; fn neg(self) -> Self { Self(-self.0, -self.1, -self.2) } }
impl AddAssign for Vec3 { fn add_assign(&mut self, other: Self) { self.0 += other.0; self.1 += other.1; self.2 += other.2; } }
impl SubAssign for Vec3 { fn sub_assign(&mut self, other: Self) { self.0 -= other.0; self.1 -= other.1; self.2 -= other.2; } }
impl MulAssign<f32> for Vec3 { fn mul_assign(&mut self, s: f32) { self.0 *= s; self.1 *= s; self.2 *= s; } }

// =============================================================================
// Vec4 - For GPU uniform buffers and alignment
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Vec4(pub f32, pub f32, pub f32, pub f32);

impl Vec4 {
    pub const ZERO: Self = Self(0.0, 0.0, 0.0, 0.0);
    pub const ONE: Self = Self(1.0, 1.0, 1.0, 1.0);

    #[inline]
    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self { Self(x, y, z, w) }

    #[inline]
    pub fn xyz(&self) -> Vec3 { Vec3(self.0, self.1, self.2) }

    #[inline]
    pub fn wxyz(&self) -> Vec3 { Vec3(self.1, self.2, self.3) }
}

impl Add for Vec4 { type Output = Self; fn add(self, other: Self) -> Self { Self(self.0 + other.0, self.1 + other.1, self.2 + other.2, self.3 + other.3) } }
impl Sub for Vec4 { type Output = Self; fn sub(self, other: Self) -> Self { Self(self.0 - other.0, self.1 - other.1, self.2 - other.2, self.3 - other.3) } }
impl Mul<f32> for Vec4 { type Output = Self; fn mul(self, s: f32) -> Self { Self(self.0 * s, self.1 * s, self.2 * s, self.3 * s) } }

// =============================================================================
// Mat4 - Column-major 4x4 matrix for GPU
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Mat4(pub [f32; 16]);

impl Mat4 {
    pub const IDENTITY: Self = Self([
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]);

    #[inline]
    pub const fn new(m: [f32; 16]) -> Self { Self(m) }

    #[inline]
    pub fn identity() -> Self { Self::IDENTITY }

    #[inline]
    pub fn col(&self, i: usize) -> Vec4 {
        debug_assert!(i < 4);
        Vec4(self.0[i*4], self.0[i*4+1], self.0[i*4+2], self.0[i*4+3])
    }

    #[inline]
    pub fn transform_vec3(&self, v: Vec3) -> Vec3 {
        let m = &self.0;
        let x = v.0; let y = v.1; let z = v.2;
        Vec3(
            m[0]*x + m[4]*y + m[8]*z + m[12],
            m[1]*x + m[5]*y + m[9]*z + m[13],
            m[2]*x + m[6]*y + m[10]*z + m[14],
        )
    }

    #[inline]
    pub fn transform_dir(&self, v: Vec3) -> Vec3 {
        let m = &self.0;
        let x = v.0; let y = v.1; let z = v.2;
        Vec3(
            m[0]*x + m[4]*y + m[8]*z,
            m[1]*x + m[5]*y + m[9]*z,
            m[2]*x + m[6]*y + m[10]*z,
        )
    }

    #[inline]
    pub fn transpose(&self) -> Self {
        let m = &self.0;
        Self([
            m[0], m[4], m[8],  m[12],
            m[1], m[5], m[9],  m[13],
            m[2], m[6], m[10], m[14],
            m[3], m[7], m[11], m[15],
        ])
    }

    /// Invert a 4x4 matrix (row-major storage; returns IDENTITY when singular)
    #[inline]
    pub fn inverse(&self) -> Self {
        let m = &self.0;
        let (a00, a01, a02, a03) = (m[0], m[1], m[2], m[3]);
        let (a10, a11, a12, a13) = (m[4], m[5], m[6], m[7]);
        let (a20, a21, a22, a23) = (m[8], m[9], m[10], m[11]);
        let (a30, a31, a32, a33) = (m[12], m[13], m[14], m[15]);

        let b00 = a00 * a11 - a01 * a10;
        let b01 = a00 * a12 - a02 * a10;
        let b02 = a00 * a13 - a03 * a10;
        let b03 = a01 * a12 - a02 * a11;
        let b04 = a01 * a13 - a03 * a11;
        let b05 = a02 * a13 - a03 * a12;
        let b06 = a20 * a31 - a21 * a30;
        let b07 = a20 * a32 - a22 * a30;
        let b08 = a20 * a33 - a23 * a30;
        let b09 = a21 * a32 - a22 * a31;
        let b10 = a21 * a33 - a23 * a31;
        let b11 = a22 * a33 - a23 * a32;

        let det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08
            - b04 * b07 + b05 * b06;
        if det.abs() < 1e-8 {
            return Self::IDENTITY;
        }
        let inv = 1.0 / det;

        Self([
            (a11 * b11 - a12 * b10 + a13 * b09) * inv,
            (a02 * b10 - a01 * b11 - a03 * b09) * inv,
            (a31 * b05 - a32 * b04 + a33 * b03) * inv,
            (a22 * b04 - a21 * b05 - a23 * b03) * inv,
            (a12 * b08 - a10 * b11 - a13 * b07) * inv,
            (a00 * b11 - a02 * b08 + a03 * b07) * inv,
            (a32 * b02 - a30 * b05 - a33 * b01) * inv,
            (a20 * b05 - a22 * b02 + a23 * b01) * inv,
            (a10 * b10 - a11 * b08 + a13 * b06) * inv,
            (a01 * b08 - a00 * b10 - a03 * b06) * inv,
            (a30 * b04 - a31 * b02 + a33 * b00) * inv,
            (a21 * b02 - a20 * b04 - a23 * b00) * inv,
            (a11 * b07 - a10 * b09 - a12 * b06) * inv,
            (a00 * b09 - a01 * b07 + a02 * b06) * inv,
            (a31 * b01 - a30 * b03 - a32 * b00) * inv,
            (a20 * b03 - a21 * b01 + a22 * b00) * inv,
        ])
    }
}

impl Default for Mat4 { fn default() -> Self { Self::IDENTITY } }

impl Mul<Mat4> for Mat4 {
    type Output = Self;
    fn mul(self, rhs: Mat4) -> Self {
        let a = &self.0; let b = &rhs.0;
        let mut r = [0.0f32; 16];
        for i in 0..4 {
            for j in 0..4 {
                r[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
            }
        }
        Self(r)
    }
}

impl Mul<Vec3> for Mat4 {
    type Output = Vec3;
    fn mul(self, v: Vec3) -> Vec3 { self.transform_vec3(v) }
}

// =============================================================================
// Matrix construction helpers
// =============================================================================

impl Mat4 {
    #[inline]
    pub fn perspective(fov_y: f32, aspect: f32, near: f32, far: f32) -> Self {
        let ymax = near * (fov_y * 0.5).tan();
        let xmax = ymax * aspect;
        let width = 2.0 * xmax;
        let height = 2.0 * ymax;
        let depth = far - near;
        let q = -(far + near) / depth;
        let qn = -(2.0 * far * near) / depth;

        Self([
            width/0.0, 0.0, 0.0, 0.0,
            0.0, height/0.0, 0.0, 0.0,
            0.0, 0.0, q, -1.0,
            0.0, 0.0, qn, 0.0,
        ])
    }

    #[inline]
    pub fn perspective_safe(fov_y: f32, aspect: f32, near: f32, far: f32) -> Self {
        let ymax = near * (fov_y * 0.5).tan();
        let xmax = ymax * aspect;
        let width = 2.0 * xmax;
        let height = 2.0 * ymax;
        let depth = far - near;
        let q = -(far + near) / depth;
        let qn = -(2.0 * far * near) / depth;

        Self([
            2.0*near/width, 0.0, 0.0, 0.0,
            0.0, 2.0*near/height, 0.0, 0.0,
            0.0, 0.0, q, -1.0,
            0.0, 0.0, qn, 0.0,
        ])
    }

    #[inline]
    pub fn look_at(eye: Vec3, target: Vec3, up: Vec3) -> Self {
        let f = (target - eye).normalized();
        let s = f.cross(up).normalized();
        let u = s.cross(f);

        Self([
            s.0, u.0, -f.0, 0.0,
            s.1, u.1, -f.1, 0.0,
            s.2, u.2, -f.2, 0.0,
            -s.dot(eye), -u.dot(eye), f.dot(eye), 1.0,
        ])
    }

    #[inline]
    pub fn ortho(left: f32, right: f32, bottom: f32, top: f32, near: f32, far: f32) -> Self {
        let r = right - left;
        let t = top - bottom;
        let f = far - near;
        Self([
            2.0/r, 0.0, 0.0, -(right+left)/r,
            0.0, 2.0/t, 0.0, -(top+bottom)/t,
            0.0, 0.0, -2.0/f, -(far+near)/f,
            0.0, 0.0, 0.0, 1.0,
        ])
    }

    #[inline]
    pub fn translate(x: f32, y: f32, z: f32) -> Self {
        Self([
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            x, y, z, 1.0,
        ])
    }

    #[inline]
    pub fn scale(x: f32, y: f32, z: f32) -> Self {
        Self([
            x, 0.0, 0.0, 0.0,
            0.0, y, 0.0, 0.0,
            0.0, 0.0, z, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ])
    }

    #[inline]
    pub fn rotate_x(a: f32) -> Self {
        let (s, c) = a.sin_cos();
        Self([
            1.0, 0.0, 0.0, 0.0,
            0.0, c, s, 0.0,
            0.0, -s, c, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ])
    }

    #[inline]
    pub fn rotate_y(a: f32) -> Self {
        let (s, c) = a.sin_cos();
        Self([
            c, 0.0, -s, 0.0,
            0.0, 1.0, 0.0, 0.0,
            s, 0.0, c, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ])
    }

    #[inline]
    pub fn rotate_z(a: f32) -> Self {
        let (s, c) = a.sin_cos();
        Self([
            c, s, 0.0, 0.0,
            -s, c, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ])
    }
}

// =============================================================================
// Bounding Box
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Bbox {
    pub min: Vec3,
    pub max: Vec3,
}

impl Bbox {
    #[inline]
    pub const fn new(min: Vec3, max: Vec3) -> Self { Self { min, max } }

    #[inline]
    pub fn contains(&self, p: Vec3) -> bool {
        p.0 >= self.min.0 && p.1 >= self.min.1 && p.2 >= self.min.2
            && p.0 <= self.max.0 && p.1 <= self.max.1 && p.2 <= self.max.2
    }

    #[inline]
    pub fn intersects(&self, origin: Vec3, dir: Vec3) -> Option<(f32, f32)> {
        let mut tmin = -f32::INFINITY;
        let mut tmax = f32::INFINITY;
        let o = [origin.0, origin.1, origin.2];
        let d = [dir.0, dir.1, dir.2];
        let mn = [self.min.0, self.min.1, self.min.2];
        let mx = [self.max.0, self.max.1, self.max.2];

        for i in 0..3 {
            if d[i].abs() < 1e-12 {
                // Ray parallel to this axis: must lie within the slab
                if o[i] < mn[i] || o[i] > mx[i] {
                    return None;
                }
            } else {
                let recip = 1.0 / d[i];
                let mut t0 = (mn[i] - o[i]) * recip;
                let mut t1 = (mx[i] - o[i]) * recip;
                if t0 > t1 {
                    std::mem::swap(&mut t0, &mut t1);
                }
                tmin = tmin.max(t0);
                tmax = tmax.min(t1);
                if tmin > tmax {
                    return None;
                }
            }
        }
        Some((tmin, tmax))
    }
}

// =============================================================================
// Intersection result
// =============================================================================

#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct HitInfo {
    pub t: f32,
    pub u: f32,
    pub v: f32,
    pub normal: Vec3,
    pub material_id: u32,
}

impl HitInfo {
    #[inline]
    pub fn miss() -> Self {
        Self { t: -1.0, u: 0.0, v: 0.0, normal: Vec3::ZERO, material_id: u32::MAX }
    }
    #[inline]
    pub const fn hit(t: f32, u: f32, v: f32, normal: Vec3, mat_id: u32) -> Self {
        Self { t, u, v, normal, material_id: mat_id }
    }
}

// =============================================================================
// Ray
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Ray {
    pub origin: Vec3,
    pub direction: Vec3,
    pub t_min: f32,
    pub t_max: f32,
}

impl Ray {
    #[inline]
    pub const fn new(origin: Vec3, direction: Vec3) -> Self {
        Self { origin, direction, t_min: 1e-8, t_max: f32::INFINITY }
    }

    #[inline]
    pub fn at(&self, t: f32) -> Vec3 {
        self.origin + self.direction * t
    }
}

// =============================================================================
// Random number generator (PCG for path tracing)
// =============================================================================

#[derive(Clone, Copy)]
pub struct Rng {
    state: u64,
}

impl Rng {
    #[inline]
    pub fn new(seed: u64) -> Self {
        Self { state: seed.wrapping_add(0xda39a3ee5e6b4b0d) }
    }

    #[inline]
    pub fn next_u32(&mut self) -> u32 {
        let prev = self.state;
        self.state = prev.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        ((prev >> 22) ^ prev) as u32
    }

    #[inline]
    pub fn next_f32(&mut self) -> f32 {
        (self.next_u32() >> 8) as f32 * (1.0 / 8388608.0)
    }

    #[inline]
    pub fn next_vec3(&mut self) -> Vec3 {
        Vec3(self.next_f32(), self.next_f32(), self.next_f32())
    }

    /// Random point in unit disk
    #[inline]
    pub fn random_disk(&mut self) -> Vec3 {
        let angle = self.next_f32() * 2.0 * core::f32::consts::PI;
        let r = self.next_f32().sqrt();
        Vec3(r * angle.cos(), r * angle.sin(), 0.0)
    }
}

// =============================================================================
// Scene data structures
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Triangle {
    pub v0: Vec3,
    pub v1: Vec3,
    pub v2: Vec3,
    pub normal: Vec3,
    pub material_id: u32,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Sphere {
    pub center: Vec3,
    pub radius: f32,
    pub material_id: u32,
    pub _pad: [f32; 3],
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Light {
    pub position: Vec3,
    pub intensity: f32,
    pub color: Vec3,
    pub _pad: f32,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Transform {
    pub translation: Vec3,
    pub scale: Vec3,
    pub rotation: f32,
    pub _pad: f32,
}

// =============================================================================
// Camera
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Camera {
    pub position: Vec3,
    pub rotation: Vec2,  // yaw, pitch
    pub fov: f32,
    pub near_plane: f32,
    pub far_plane: f32,
    pub aspect: f32,
    pub exposure: f32,
    pub _pad: [f32; 3],
}

impl Camera {
    #[inline]
    pub fn view_matrix(&self) -> Mat4 {
        Mat4::look_at(self.position, self.position + self.forward(), Vec3::Y)
    }

    #[inline]
    pub fn projection_matrix(&self) -> Mat4 {
        Mat4::perspective_safe(self.fov, self.aspect, self.near_plane, self.far_plane)
    }

    #[inline]
    pub fn forward(&self) -> Vec3 {
        let (cy, sy) = self.rotation.0.sin_cos();
        let (cp, sp) = self.rotation.1.sin_cos();
        Vec3(sy * cp, sp, cy * cp)
    }

    #[inline]
    pub fn ray_for_pixel(&self, px: f32, py: f32, width: f32, height: f32) -> Ray {
        let ndc_x = (2.0 * px / width - 1.0);
        let ndc_y = (1.0 - 2.0 * py / height);
        let aspect = self.aspect;

        let ray_dir = Vec3(ndc_x / aspect, ndc_y, -1.0).normalized();
        let view = self.view_matrix();
        let dir = view.transform_dir(ray_dir);

        Ray::new(self.position, dir.normalized())
    }
}

// =============================================================================
// Materials
// =============================================================================

#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct Material {
    pub albedo: Vec3,
    pub roughness: f32,
    pub metallic: f32,
    pub ior: f32,
    pub emissive: Vec3,
    pub light_intensity: f32,
    pub _pad: [f32; 3],
}

impl Material {
    #[inline]
    pub fn diffuse(albedo: Vec3) -> Self {
        Self { albedo, ..Default::default() }
    }

    #[inline]
    pub fn metal(albedo: Vec3, roughness: f32) -> Self {
        Self { albedo, roughness, metallic: 1.0, ..Default::default() }
    }

    #[inline]
    pub fn emissive(color: Vec3, intensity: f32) -> Self {
        Self { emissive: color, light_intensity: intensity, ..Default::default() }
    }

    #[inline]
    pub fn glass(ior: f32) -> Self {
        Self { ior, ..Default::default() }
    }
}





impl std::ops::Index<usize> for Vec3 {
    type Output = f32;
    fn index(&self, i: usize) -> &f32 {
        match i {
            0 => &self.0,
            1 => &self.1,
            2 => &self.2,
            _ => panic!("Vec3 index out of range"),
        }
    }
}

impl std::ops::IndexMut<usize> for Vec3 {
    fn index_mut(&mut self, i: usize) -> &mut f32 {
        match i {
            0 => &mut self.0,
            1 => &mut self.1,
            2 => &mut self.2,
            _ => panic!("Vec3 index out of range"),
        }
    }
}

impl Vec3 {
    /// Squared length (cheaper than length for comparisons).
    pub fn length_squared(&self) -> f32 {
        self.0 * self.0 + self.1 * self.1 + self.2 * self.2
    }
}

impl Default for Camera {
    fn default() -> Self {
        Self {
            position: Vec3::ZERO,
            rotation: Vec2::ZERO,
            fov: 90.0,
            near_plane: 0.1,
            far_plane: 1000.0,
            aspect: 16.0 / 9.0,
            exposure: 1.0,
            _pad: [0.0; 3],
        }
    }
}

impl Bbox {
    /// Center point of the box.
    pub fn center(&self) -> Vec3 {
        Vec3::new(
            (self.min.0 + self.max.0) * 0.5,
            (self.min.1 + self.max.1) * 0.5,
            (self.min.2 + self.max.2) * 0.5,
        )
    }

    /// Full extent per axis.
    pub fn size(&self) -> Vec3 {
        Vec3::new(
            self.max.0 - self.min.0,
            self.max.1 - self.min.1,
            self.max.2 - self.min.2,
        )
    }

    /// Build a box from a center and full extent.
    pub fn from_center_size(center: Vec3, size: Vec3) -> Self {
        let half = Vec3::new(size.0 * 0.5, size.1 * 0.5, size.2 * 0.5);
        Self::new(center - half, center + half)
    }

    /// Slab ray intersection test limited to `max_t`.
    pub fn intersects_ray(&self, origin: Vec3, dir: Vec3, max_t: f32) -> bool {
        match self.intersects(origin, dir) {
            Some((tmin, tmax)) => tmax >= 0.0 && tmin <= max_t,
            None => false,
        }
    }
}

impl Bbox {
    /// True when the two boxes share any volume.
    pub fn overlaps(&self, other: &Bbox) -> bool {
        self.min.0 <= other.max.0 && other.min.0 <= self.max.0
            && self.min.1 <= other.max.1 && other.min.1 <= self.max.1
            && self.min.2 <= other.max.2 && other.min.2 <= self.max.2
    }
}

impl Bbox {
    /// Degenerate box covering nothing (min = +inf, max = -inf).
    pub const EMPTY: Self = Self {
        min: Vec3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY),
        max: Vec3::new(f32::NEG_INFINITY, f32::NEG_INFINITY, f32::NEG_INFINITY),
    };

    /// Smallest box containing both inputs.
    pub fn merge(&self, other: &Bbox) -> Bbox {
        Bbox::new(
            Vec3::new(
                self.min.0.min(other.min.0),
                self.min.1.min(other.min.1),
                self.min.2.min(other.min.2),
            ),
            Vec3::new(
                self.max.0.max(other.max.0),
                self.max.1.max(other.max.1),
                self.max.2.max(other.max.2),
            ),
        )
    }
}
