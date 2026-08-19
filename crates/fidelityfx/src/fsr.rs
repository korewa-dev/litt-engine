//! AMD FidelityFX Super Resolution (FSR) integration.
//! FSR 2 for temporal upscaling + FSR 3 for frame generation.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// FSR 2 constants for the compute shader
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr2Constants {
    pub input_width: u32,
    pub input_height: u32,
    pub output_width: u32,
    pub output_height: u32,
    pub exposure: f32,
    pub sharpness: f32,
    pub motion_scale: f32,
    pub _pad: f32,
    pub frame_time: f32,
    pub _pad2: [f32; 3],
}

/// FSR 2 state (reusable across frames)
#[derive(Debug)]
pub struct Fsr2State {
    pub constants: Fsr2Constants,
    pub is_ready: bool,
    pub last_frame_time: f32,
}

impl Fsr2State {
    pub fn new(input_w: u32, input_h: u32, output_w: u32, output_h: u32) -> Self {
        Self {
            constants: Fsr2Constants {
                input_width: input_w,
                input_height: input_h,
                output_width: output_w,
                output_height: output_h,
                ..Default::default()
            },
            is_ready: false,
            last_frame_time: 1.0 / 60.0,
        }
    }

    pub fn update(&mut self, frame_time: f32, exposure: f32) {
        self.last_frame_time = frame_time;
        self.constants.frame_time = frame_time;
        self.constants.exposure = exposure;
    }
}

impl Default for Fsr2Constants {
    fn default() -> Self {
        Self {
            input_width: 0,
            input_height: 0,
            output_width: 0,
            output_height: 0,
            exposure: 1.0,
            sharpness: 0.5,
            motion_scale: 1.0,
            _pad: 0.0,
            frame_time: 1.0 / 60.0,
            _pad2: [0.0; 3],
        }
    }
}

/// FSR 3 frame generation constants
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
            create_width: 0,
            create_height: 0,
            compensate_width: 0,
            compensate_height: 0,
            upscaler_width: 0,
            upscaler_height: 0,
            framegen_width: 0,
            framegen_height: 0,
            exposure: 1.0,
            frame_ratio: 1.0,
            sharpeness: 0.5,
            _pad: 0.0,
        }
    }
}
