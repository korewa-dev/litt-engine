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


// =============================================================================
// FSR 4 Support (AMD's latest - includes all FSR features + AI enhancements)
// =============================================================================

/// FSR 4 quality presets (superset of FSR 3)
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Quality {
    #[default]
    UltraQuality,  // 0.56x - highest quality
    Quality,       // 0.67x
    Balanced,      // 0.83x
    Performance,   // 1.0x
    UltraPerformance, // 1.5x - lowest resolution
}

/// FSR 4 mode (all FSR 3 modes + AI reconstruction)
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Mode {
    #[default]
    Upscale,           // Spatial + temporal upscaling
    FrameGen,          // Upscaling + frame generation
    AiReconstruction,  // Full FSR 4 with AI reconstruction (RDNA 4/5)
}

/// FSR 4 configuration (extends Fsr3Constants)
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr4Constants {
    // FSR 3 fields
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
    // FSR 4 additions
    pub ai_reconstruction: u32,
    pub temporal_stability: f32,
    pub quality_preset: u32,  // Fsr4Quality
    pub mode: u32,            // Fsr4Mode
}

impl Default for Fsr4Constants {
    fn default() -> Self {
        Self {
            create_width: 0, create_height: 0,
            compensate_width: 0, compensate_height: 0,
            upscaler_width: 0, upscaler_height: 0,
            framegen_width: 0, framegen_height: 0,
            exposure: 1.0, frame_ratio: 1.0, sharpeness: 0.25, _pad: 0.0,
            ai_reconstruction: 0,
            temporal_stability: 0.5,
            quality_preset: 0,
            mode: 0,
        }
    }
}

/// FSR 4 state (extends Fsr3State)
#[derive(Debug)]
pub struct Fsr4State {
    pub constants: Fsr4Constants,
    pub is_ready: bool,
    pub support_level: Fsr4Support,
}

/// GPU support level for FSR 4
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Support {
    #[default]
    None,
    Fsr1,        // Basic spatial only
    Fsr2,        // Temporal upscaling
    Fsr3,        // Temporal + frame gen
    Fsr4,        // Full AI-enhanced (RDNA 4/5)
}

impl Fsr4State {
    pub fn new(input_w: u32, input_h: u32, output_w: u32, output_h: u32) -> Self {
        Self {
            constants: Fsr4Constants {
                create_width: input_w, create_height: input_h,
                upscaler_width: output_w, upscaler_height: output_h,
                framegen_width: output_w, framegen_height: output_h,
                ..Default::default()
            },
            is_ready: false,
            support_level: Fsr4Support::Fsr2, // Default to FSR 2
        }
    }

    /// Detect GPU capability for FSR version
    pub fn detect_support(vendor_id: u32, device_name: &str) -> Fsr4Support {
        let name_lower = device_name.to_lowercase();
        
        // RDNA 4/5 - full FSR 4 support
        if vendor_id == 0x1002 {
            if name_lower.contains("rdna 4") || name_lower.contains("rdna4") ||
               name_lower.contains("9000") || name_lower.contains("npu") {
                return Fsr4Support::Fsr4;
            }
            // RDNA 3
            if name_lower.contains("rdna 3") || name_lower.contains("rdna3") {
                return Fsr4Support::Fsr3;
            }
            return Fsr4Support::Fsr3;
        }
        
        // Intel Arc - FSR 3 support
        if vendor_id == 0x8086 {
            return Fsr4Support::Fsr3;
        }
        
        // Samsung Exynos - FSR 3 support
        if vendor_id == 0x1AE {
            return Fsr4Support::Fsr3;
        }
        
        // Moore Threads - FSR 2 support
        if vendor_id == 0x1DD {
            return Fsr4Support::Fsr2;
        }
        
        // Mobile GPUs - FSR 2 or FSR 3
        if vendor_id == 0x5143 { // Qualcomm
            return Fsr4Support::Fsr3;
        }
        
        // MediaTek, Kirin - FSR 2
        return Fsr4Support::Fsr2;
    }

    pub fn update(&mut self, quality: Fsr4Quality, mode: Fsr4Mode, ai_recon: bool, frame_gen: bool) {
        self.constants.quality_preset = quality as u32;
        self.constants.mode = mode as u32;
        self.constants.ai_reconstruction = if ai_recon { 1 } else { 0 };
        self.is_ready = true;
    }
}
