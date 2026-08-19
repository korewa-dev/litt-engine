//! AMD FidelityFX Super Resolution 3 integration.
//! FSR 3 frame generation for temporal upscaling + frame interpolation.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// FSR 3 constants for the compute shader
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr3Constants {
    pub create_width: u32,
    pub create_height: u32,
    pub compensate_width: u32,
    pub compensate_height: u32,
    pub upscaler_width: u32,
    pub upscaler_height: u32,
    pub framegen_width: u32,
    pub framegen_height: u32,
    pub exposure: f32,
    pub frame_ratio: f32,
    pub sharpeness: f32,
    pub _pad: f32,
}

impl Default for Fsr3Constants {
    fn default() -> Self {
        Self {
            create_width: 0, create_height: 0,
            compensate_width: 0, compensate_height: 0,
            upscaler_width: 0, upscaler_height: 0,
            framegen_width: 0, framegen_height: 0,
            exposure: 1.0, frame_ratio: 1.0, sharpeness: 0.5, _pad: 0.0,
        }
    }
}

/// FSR 3 state (reusable across frames)
#[derive(Debug)]
pub struct Fsr3State {
    pub constants: Fsr3Constants,
    pub is_ready: bool,
}

impl Fsr3State {
    pub fn new(input_w: u32, input_h: u32, output_w: u32, output_h: u32) -> Self {
        Self {
            constants: Fsr3Constants {
                create_width: input_w, create_height: input_h,
                upscaler_width: output_w, upscaler_height: output_h,
                framegen_width: output_w, framegen_height: output_h,
                ..Default::default()
            },
            is_ready: false,
        }
    }
}
