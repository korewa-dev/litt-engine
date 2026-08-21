//! Material system — PBR (Physically Based Rendering) materials.
//! Supports diffuse, specular, metallic, roughness, emission, and normal mapping.

use litt_math::Vec3;
use super::handle::AssetHandle;
use super::texture::Texture;

/// Material type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MaterialType {
    /// Standard PBR
    Pbr,
    /// Unlit (no lighting)
    Unlit,
    /// Transparent
    Transparent,
    /// Emissive
    Emissive,
    /// Custom
    Custom(String),
}

/// Blend mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BlendMode {
    /// Opaque
    Opaque,
    /// Alpha blend
    AlphaBlend,
    /// Additive
    Additive,
    /// Multiplicative
    Multiplicative,
}

/// Culling mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CullMode {
    None,
    Front,
    Back,
}

/// A PBR material
#[derive(Debug)]
pub struct Material {
    pub handle: AssetHandle,
    pub name: String,
    pub material_type: MaterialType,
    pub blend_mode: BlendMode,
    pub cull_mode: CullMode,
    pub double_sided: bool,
    pub depth_write: bool,
    pub depth_compare: bool,
    /// Diffuse/albedo color
    pub albedo: Vec3,
    /// Metalness (0 = dielectric, 1 = metal)
    pub metallic: f32,
    /// Roughness (0 = mirror, 1 = diffuse)
    pub roughness: f32,
    /// Specular intensity
    pub specular: Vec3,
    /// Fresnel IOR
    pub ior: f32,
    /// Emissive color
    pub emissive: Vec3,
    /// Emissive intensity
    pub emissive_intensity: f32,
    /// Normal strength
    pub normal_strength: f32,
    /// Texture handles
    pub albedo_map: Option<AssetHandle>,
    pub metallic_roughness_map: Option<AssetHandle>,
    pub normal_map: Option<AssetHandle>,
    pub emissive_map: Option<AssetHandle>,
    pub occlusion_map: Option<AssetHandle>,
}

impl Default for Material {
    fn default() -> Self {
        Self::new("default")
    }
}

impl Material {
    /// Create a new default material
    pub fn new(name: &str) -> Self {
        Self {
            handle: AssetHandle::new(0, super::handle::AssetType::Material),
            name: name.to_string(),
            material_type: MaterialType::Pbr,
            blend_mode: BlendMode::Opaque,
            cull_mode: CullMode::Back,
            double_sided: false,
            depth_write: true,
            depth_compare: true,
            albedo: Vec3::new(0.8, 0.8, 0.8),
            metallic: 0.0,
            roughness: 0.5,
            specular: Vec3::new(0.5, 0.5, 0.5),
            ior: 1.5,
            emissive: Vec3::ZERO,
            emissive_intensity: 0.0,
            normal_strength: 1.0,
            albedo_map: None,
            metallic_roughness_map: None,
            normal_map: None,
            emissive_map: None,
            occlusion_map: None,
        }
    }

    /// Create a metal material
    pub fn metal(albedo: Vec3, roughness: f32) -> Self {
        Self {
            albedo,
            metallic: 1.0,
            roughness,
            specular: albedo,
            ..Self::new("metal")
        }
    }

    /// Create a dielectric (non-metal) material
    pub fn dielectric(albedo: Vec3, roughness: f32, ior: f32) -> Self {
        Self {
            albedo,
            metallic: 0.0,
            roughness,
            ior,
            ..Self::new("dielectric")
        }
    }

    /// Create an emissive material
    pub fn emissive(color: Vec3, intensity: f32) -> Self {
        Self {
            material_type: MaterialType::Emissive,
            emissive: color,
            emissive_intensity: intensity,
            ..Self::new("emissive")
        }
    }

    /// Create a transparent material
    pub fn transparent(albedo: Vec3, opacity: f32) -> Self {
        Self {
            material_type: MaterialType::Transparent,
            blend_mode: BlendMode::AlphaBlend,
            albedo,
            emissive: albedo * opacity,
            ..Self::new("transparent")
        }
    }

    /// Create an unlit material
    pub fn unlit(albedo: Vec3) -> Self {
        Self {
            material_type: MaterialType::Unlit,
            albedo,
            ..Self::new("unlit")
        }
    }

    /// Get the effective albedo considering textures
    pub fn effective_albedo(&self) -> Vec3 {
        self.albedo * (1.0 - self.metallic) + self.specular * self.metallic
    }
}

/// Material factory — creates common materials
pub struct MaterialFactory;

impl MaterialFactory {
    /// Create a concrete material
    pub fn concrete() -> Material {
        Material {
            albedo: Vec3::new(0.7, 0.7, 0.7),
            metallic: 0.0,
            roughness: 0.9,
            ..Material::new("concrete")
        }
    }

    /// Create a metal material
    pub fn steel() -> Material {
        Material {
            albedo: Vec3::new(0.8, 0.8, 0.85),
            metallic: 1.0,
            roughness: 0.4,
            specular: Vec3::new(0.8, 0.8, 0.85),
            ..Material::new("steel")
        }
    }

    /// Create a gold material
    pub fn gold() -> Material {
        Material {
            albedo: Vec3::new(1.0, 0.84, 0.5),
            metallic: 1.0,
            roughness: 0.2,
            specular: Vec3::new(1.0, 0.84, 0.5),
            ..Material::new("gold")
        }
    }

    /// Create a copper material
    pub fn copper() -> Material {
        Material {
            albedo: Vec3::new(1.0, 0.55, 0.35),
            metallic: 1.0,
            roughness: 0.3,
            specular: Vec3::new(1.0, 0.55, 0.35),
            ..Material::new("copper")
        }
    }

    /// Create a glass material
    pub fn glass() -> Material {
        Material {
            material_type: MaterialType::Transparent,
            blend_mode: BlendMode::AlphaBlend,
            albedo: Vec3::new(0.9, 0.95, 1.0),
            metallic: 0.0,
            roughness: 0.05,
            ior: 1.52,
            ..Material::new("glass")
        }
    }

    /// Create an emissive light material
    pub fn light(color: Vec3, intensity: f32) -> Material {
        Material::emissive(color, intensity)
    }
}
