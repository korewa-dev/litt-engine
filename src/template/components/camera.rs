//! Camera component
//!
//! FPS/Free-fly camera with view and projection matrices.

use litt_math::{Vec3, Vec2, Mat4};

#[derive(Clone, Debug)]
pub struct Camera {
    pub position: Vec3,
    pub rotation: Vec2, // yaw, pitch
    pub fov: f32,
    pub near_plane: f32,
    pub far_plane: f32,
    pub aspect: f32,
    pub exposure: f32,
}

impl Camera {
    pub fn new() -> Self {
        Self {
            position: Vec3::new(0.0, 2.0, 5.0),
            rotation: Vec2::new(0.0, 0.0),
            fov: core::f32::consts::PI / 3.0,
            near_plane: 0.1,
            far_plane: 100.0,
            aspect: 16.0 / 9.0,
            exposure: 1.0,
        }
    }

    pub fn view_matrix(&self) -> Mat4 {
        Mat4::look_at(self.position, self.position + self.forward(), Vec3::Y)
    }

    pub fn projection_matrix(&self) -> Mat4 {
        Mat4::perspective_safe(self.fov, self.aspect, self.near_plane, self.far_plane)
    }

    pub fn forward(&self) -> Vec3 {
        let (cy, sy) = self.rotation.0.sin_cos();
        let (cp, sp) = self.rotation.1.sin_cos();
        Vec3(sy * cp, sp, cy * cp)
    }
}

impl Default for Camera {
    fn default() -> Self { Self::new() }
}
