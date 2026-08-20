//! Material component - PBR shading parameters
//!
//! GPU-compatible material data for path tracing.

use litt_math::Vec3;
use bytemuck::{Pod, Zeroable};

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
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
    pub fn diffuse(albedo: Vec3) -> Self {
        Self { albedo, ..Default::default() }
    }

    pub fn metal(albedo: Vec3, roughness: f32) -> Self {
        Self { albedo, roughness, metallic: 1.0, ..Default::default() }
    }

    pub fn emissive(color: Vec3, intensity: f32) -> Self {
        Self { emissive: color, light_intensity: intensity, ..Default::default() }
    }
}

impl Default for Material {
    fn default() -> Self {
        Self {
            albedo: Vec3::new(0.8, 0.8, 0.8),
            roughness: 0.5,
            metallic: 0.0,
            ior: 1.5,
            emissive: Vec3::ZERO,
            light_intensity: 0.0,
            _pad: [0.0; 3],
        }
    }
}
