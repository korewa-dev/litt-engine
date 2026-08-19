//! Intel XeSS 3 integration.
//! XeSS 3 combines spatial upscaling, frame generation, and AI reconstruction.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// XeSS 3 configuration
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Xess3Config {
    pub input_width: u32,
    pub input_height: u32,
    pub output_width: u32,
    pub output_height: u32,
    pub quality_level: u32, // 0=Performance, 1=Balanced, 2=Quality, 3=Ultra Quality
    pub frame_gen_enabled: bool,
    pub sharpeness: f32,
    pub _pad: f32,
}

impl Default for Xess3Config {
    fn default() -> Self {
        Self {
            input_width: 0,
            input_height: 0,
            output_width: 0,
            output_height: 0,
            quality_level: 2, // Quality
            frame_gen_enabled: true,
            sharpeness: 0.5,
            _pad: 0.0,
        }
    }
}

/// XeSS 3 state
#[derive(Debug)]
pub struct Xess3 {
    pub config: Xess3Config,
    pub is_ready: bool,
}

impl Xess3 {
    pub fn new(input_w: u32, input_h: u32, output_w: u32, output_h: u32) -> Self {
        Self {
            config: Xess3Config {
                input_width: input_w,
                input_height: input_h,
                output_width: output_w,
                output_height: output_h,
                ..Default::default()
            },
            is_ready: false,
        }
    }

    pub fn update(&mut self, quality: u32, frame_gen: bool) {
        self.config.quality_level = quality.min(3);
        self.config.frame_gen_enabled = frame_gen;
        self.is_ready = true;
    }
}
