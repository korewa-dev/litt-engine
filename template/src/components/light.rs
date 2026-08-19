//! Light component
use litt_math::*;
use bytemuck::{Pod, Zeroable};

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Light {
    pub position: Vec3,
    pub direction: Vec3,
    pub color: Vec3,
    pub intensity: f32,
    pub radius: f32,
    pub _pad: [f32; 2],
}

impl Light {
    pub fn point(pos: Vec3, color: Vec3, intensity: f32) -> Self {
        Self { position: pos, direction: Vec3::ZERO, color, intensity, radius: 0.0, _pad: [0.0;2] }
    }
    pub fn directional(dir: Vec3, color: Vec3, intensity: f32) -> Self {
        Self { position: Vec3::ZERO, direction: dir.normalized(), color, intensity, radius: 0.0, _pad: [0.0;2] }
    }
}

impl Default for Light {
    fn default() -> Self {
        Self { position: Vec3::new(0.0,8.0,-5.0), direction: Vec3::ZERO,
               color: Vec3::new(1.0,0.95,0.9), intensity: 50.0, radius: 2.0, _pad: [0.0;2] }
    }
}
