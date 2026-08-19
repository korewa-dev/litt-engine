//! FidelityFX Ray Reconstruction integration.
//! Uses a lightweight CNN to reconstruct path-traced images.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// Ray reconstruction constants
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct RayReconConstants {
    pub width: u32,
    pub height: u32,
    pub temporal_scale: f32,
    pub blend: f32,
    pub confidence_threshold: f32,
    pub _pad: f32,
}

impl Default for RayReconConstants {
    fn default() -> Self {
        Self {
            width: 0,
            height: 0,
            temporal_scale: 0.5,
            blend: 0.5,
            confidence_threshold: 0.5,
            _pad: 0.0,
        }
    }
}

/// Ray Reconstruction state
#[derive(Debug)]
pub struct RayReconstruction {
    pub constants: RayReconConstants,
    pub is_ready: bool,
    pub use_temporal: bool,
}

impl RayReconstruction {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            constants: RayReconConstants { width, height, ..Default::default() },
            is_ready: false,
            use_temporal: true,
        }
    }

    pub fn update(&mut self, temporal_scale: f32, blend: f32) {
        self.constants.temporal_scale = temporal_scale;
        self.constants.blend = blend;
        self.is_ready = true;
    }
}
