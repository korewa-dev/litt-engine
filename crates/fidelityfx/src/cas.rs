//! AMD FidelityFX Contrast Adaptive Sharpening (CAS).
//! Fast, high-quality sharpening for the final image.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// CAS constants
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct CasConstants {
    pub width: u32,
    pub height: u32,
    pub sharpening: f32,
    pub _pad: f32,
}

impl Default for CasConstants {
    fn default() -> Self {
        Self {
            width: 0,
            height: 0,
            sharpening: 0.25,
            _pad: 0.0,
        }
    }
}

/// CAS state
#[derive(Debug)]
pub struct Cas {
    pub constants: CasConstants,
    pub is_ready: bool,
}

impl Cas {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            constants: CasConstants { width, height, ..Default::default() },
            is_ready: false,
        }
    }

    pub fn update(&mut self, sharpening: f32) {
        self.constants.sharpening = sharpening.min(1.0).max(0.0);
        self.is_ready = true;
    }
}
