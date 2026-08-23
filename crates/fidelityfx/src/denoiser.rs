//! FidelityFX denoiser integration.
//! Diffuse and specular denoisers for path tracing.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// Denoiser constants
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct DenoiserConstants {
    pub width: u32,
    pub height: u32,
    pub temporal_scale: f32,
    pub blend: f32,
    pub hit_rate: f32,
    pub _pad: f32,
}

impl Default for DenoiserConstants {
    fn default() -> Self {
        Self {
            width: 0,
            height: 0,
            temporal_scale: 0.5,
            blend: 0.5,
            hit_rate: 1.0,
            _pad: 0.0,
        }
    }
}

/// FidelityFX Diffuse Denoiser state
#[derive(Debug)]
pub struct DiffuseDenoiser {
    pub constants: DenoiserConstants,
    pub is_ready: bool,
}

impl DiffuseDenoiser {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            constants: DenoiserConstants {
                width,
                height,
                ..Default::default()
            },
            is_ready: false,
        }
    }

    pub fn update(&mut self, temporal_scale: f32, blend: f32) {
        self.constants.temporal_scale = temporal_scale;
        self.constants.blend = blend;
        self.is_ready = true;
    }
}

/// FidelityFX Specular Denoiser state
#[derive(Debug)]
pub struct SpecularDenoiser {
    pub constants: DenoiserConstants,
    pub is_ready: bool,
}

impl SpecularDenoiser {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            constants: DenoiserConstants {
                width,
                height,
                ..Default::default()
            },
            is_ready: false,
        }
    }
}
