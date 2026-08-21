//! AMD FidelityFX Contrast Adaptive Sharpening (CAS).
//!
//! The real CAS pipeline is defined in `fsr.rs` (CasPipeline).
//! This module re-exports for backwards compatibility.

pub use super::fsr::CasConstants;
pub use super::fsr::CasPipeline;

/// Legacy CAS state (thin wrapper around CasPipeline for API compat)
#[derive(Debug)]
pub struct Cas {
    pub sharpening: f32,
    pub is_ready: bool,
}

impl Cas {
    pub fn new(_width: u32, _height: u32) -> Self {
        Self {
            sharpening: 0.25,
            is_ready: false,
        }
    }

    pub fn update(&mut self, sharpening: f32) {
        self.sharpening = sharpening.min(1.0).max(0.0);
        self.is_ready = true;
    }
}
