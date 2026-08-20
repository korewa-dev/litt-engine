//! Ultra-lightweight math types for the path tracer.
//! No external math library â€” hand-rolled SIMD-friendly types.
//!
//! Zero-cost abstractions, no heap allocation, no trait objects.

#![no_std]
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

    /// Invert a 4x4 matrix (used for view matrices)
    #[inline]
    pub fn inverse(&self) -> Self {
        let m = &self.0;
        let cof0 = m[10]*m[15] - m[14]*m[11];
        let cof1 = m[11]*m[14] - m[9]*m[15];
        let cof2 = m[9]*m[11] - m[10]*m[14];
        let cof3 = m[10]*m[13] - m[13]*m[11];
        let cof4 = m[11]*m[12] - m[9]*m[13];
        let cof5 = m[9]*m[13] - m[10]*m[12];
        let cof6 = m[6]*m[15] - m[14]*m[7];
        let cof7 = m[7]*m[14] - m[5]*m[15];
        let cof8 = m[5]*m[14] - m[6]*m[13];
        let cof9 = m[6]*m[13] - m[7]*m[12];
        let cof10 = m[7]*m[12] - m[5]*m[13];
        let cof11 = m[5]*m[13] - m[6]*m[12];
        let cof12 = m[2]*m[15] - m[14]*m[3];
        let cof13 = m[3]*m[14] - m[1]*m[15];
        let cof14 = m[1]*m[14] - m[2]*m[13];
        let cof15 = m[2]*m[13] - m[3]*m[12];
        let cof16 = m[6]*m[11] - m[10]*m[7];
        let cof17 = m[7]*m[10] - m[5]*m[11];
        let cof18 = m[5]*m[11] - m[6]*m[10];
        let cof19 = m[6]*m[9] - m[5]*m[8];
        let cof20 = m[7]*m[8] - m[5]*m[9];
        let cof21 = m[5]*m[9] - m[6]*m[8];
        let cof22 = m[2]*m[11] - m[10]*m[3];
        let cof23 = m[3]*m[10] - m[1]*m[11];
        let cof24 = m[1]*m[11] - m[2]*m[10];
        let cof25 = m[2]*m[9] - m[1]*m[8];
        let cof26 = m[3]*m[8] - m[1]*m[9];
        let cof27 = m[1]*m[9] - m[2]*m[8];

        let det = m[0]*cof0 - m[1]*cof1 + m[2]*cof2 - m[3]*cof3
                + m[4]*cof4 - m[5]*cof5 + m[6]*cof6 - m[7]*cof7
                + m[8]*cof8 - m[9]*cof9 + m[10]*cof10 - m[11]*cof11
                + m[12]*cof12 - m[13]*cof13 + m[14]*cof14 - m[15]*cof15;

        if det.abs() < 1e-8 { return Self::IDENTITY; }
        let inv_det = 1.0 / det;

        Self([
            cof0*inv_det,  cof1*inv_det,  cof2*inv_det,  cof3*inv_det,
            cof4*inv_det,  cof5*inv_det,  cof6*inv_det,  cof7*inv_det,
            cof8*inv_det,  cof9*inv_det,  cof10*inv_det, cof11*inv_det,
            cof12*inv_det, cof13*inv_det, cof14*inv_det, cof15*inv_det,
            cof16*inv_det, cof17*inv_det, cof18*inv_det, cof19*inv_det,
            cof20*inv_det, cof21*inv_det, cof22*inv_det, cof23*inv_det,
            cof24*inv_det, cof25*inv_det, cof26*inv_det, cof27*inv_det,
            (-m[4]*cof1 + m[5]*cof0 - m[1]*cof4 + m[0]*cof5
              + m[6]*cof3 - m[7]*cof2 + m[3]*cof6 - m[2]*cof7
              + m[8]*cof1 - m[9]*cof0 + m[1]*cof8 - m[0]*cof9
              + m[10]*cof2 - m[11]*cof1 + m[3]*cof10 - m[2]*cof11
              + m[4]*cof5 - m[5]*cof4 + m[0]*cof9 - m[1]*cof8
              + m[6]*cof7 - m[7]*cof6 + m[2]*cof11 - m[3]*cof10
              + m[8]*cof3 - m[9]*cof2 + m[1]*cof6 - m[0]*cof7
              + m[12]*cof2 - m[13]*cof1 + m[1]*cof15 - m[0]*cof14
              + m[4]*cof13 - m[5]*cof12 + m[0]*cof14 - m[1]*cof13
              + m[8]*cof1 - m[9]*cof0 + m[1]*cof12 - m[0]*cof13
              + m[12]*cof5 - m[13]*cof4 + m[4]*cof14 - m[5]*cof13
              + m[12]*cof1 - m[13]*cof0 + m[0]*cof13 - m[1]*cof12
              + m[4]*cof12 - m[5]*cof11 + m[1]*cof15 - m[0]*cof14
              + m[8]*cof4 - m[9]*cof3 + m[3]*cof12 - m[2]*cof13
              + m[12]*cof4 - m[13]*cof3 + m[2]*cof15 - m[3]*cof14
              + m[8]*cof0 - m[9]*cof1 + m[1]*cof11 - m[0]*cof12
              + m[4]*cof3 - m[5]*cof2 + m[2]*cof14 - m[3]*cof13
              + m[12]*cof3 - m[13]*cof2 + m[3]*cof11 - m[2]*cof15
              + m[8]*cof2 - m[9]*cof3 + m[3]*cof13 - m[2]*cof12
              + m[12]*cof2 - m[13]*cof1 + m[1]*cof15 - m[0]*cof14
              + m[4]*cof1 - m[5]*cof0 + m[0]*cof12 - m[1]*cof11
              + m[8]*cof3 - m[9]*cof2 + m[2]*cof15 - m[3]*cof14
              + m[12]*cof1 - m[13]*cof0 + m[0]*cof13 - m[1]*cof12
              + m[4]*cof0 - m[5]*cof1 + m[1]*cof14 - m[0]*cof13
              + m[8]*cof1 - m[9]*cof0 + m[0]*cof15 - m[1]*cof14
            ) / det,
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
        let mut tmin = -1e30;
        let mut tmax = 1e30;

        for i in 0..3 {
            let (o, d, mn, mx) = (origin.0, dir.0, self.min.0, self.max.0);
            let recip = 1.0 / d;
            let t0 = (mn - o) * recip;
            let t1 = (mx - o) * recip;
            if t0 > t1 { let (a, b) = (t0, t1); t0 = b; t1 = a; }
            tmin = tmin.max(t0);
            tmax = tmax.min(t1);
            if tmin > tmax { return None; }
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
    pub fn miss() -> Self { Self { t: -1.0, normal: Vec3::ZERO, material: None } }
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


