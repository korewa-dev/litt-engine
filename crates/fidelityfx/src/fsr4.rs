//! AMD FidelityFX Super Resolution 4 support.
//! FSR 4 adds AI-enhanced upscaling for RDNA 4/5 and all Vulkan GPUs.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// FSR 4 quality presets
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Quality {
    #[default]
    UltraQuality,  // 0.56x resolution
    Quality,       // 0.67x
    Balanced,      // 0.83x
    Performance,   // 1.0x (native)
    UltraPerformance, // 1.5x
}

/// FSR 4 mode
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Mode {
    #[default]
    Upscale,       // Spatial + temporal upscaling only
    FrameGen,      // Upscale + frame generation
    Full,          // Upscale + frame gen + AI reconstruction
}

/// FSR 4 configuration
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr4Config {
    pub input_width: u32,
    pub input_height: u32,
    pub output_width: u32,
    pub output_height: u32,
    pub quality: u32,          // Fsr4Quality
    pub mode: u32,             // Fsr4Mode
    pub sharpeness: f32,       // 0.0 - 1.0
    pub contrast: f32,         // 0.5 - 2.0
    pub temporal_stability: f32, // 0.0 - 1.0
    pub ai_reconstruction: bool,
    pub frame_gen_enabled: bool,
    pub _pad: [u32; 2],
}

impl Default for Fsr4Config {
    fn default() -> Self {
        Self {
            input_width: 0, input_height: 0,
            output_width: 0, output_height: 0,
            quality: 0, // UltraQuality
            mode: 0,    // Upscale
            sharpeness: 0.25,
            contrast: 1.0,
            temporal_stability: 0.5,
            ai_reconstruction: false,
            frame_gen_enabled: false,
            _pad: [0; 2],
        }
    }
}

/// FSR 4 state
#[derive(Debug)]
pub struct Fsr4 {
    pub config: Fsr4Config,
    pub is_ready: bool,
    pub support_level: Fsr4Support,
}

/// GPU support level for FSR 4
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Support {
    #[default]
    None,
    /// Basic spatial upscaling only (all GPUs)
    Spatial,
    /// Spatial + temporal (FSR 2-class)
    Temporal,
    /// Full FSR 4 with AI reconstruction
    Full,
}

impl Fsr4 {
    pub fn new(input_w: u32, input_h: u32, output_w: u32, output_h: u32) -> Self {
        Self {
            config: Fsr4Config {
                input_width: input_w,
                input_height: input_h,
                output_width: output_w,
                output_height: output_h,
                ..Default::default()
            },
            is_ready: false,
            support_level: Fsr4Support::Spatial,
        }
    }

    /// Detect GPU capability for FSR 4
    pub fn detect_support(device: &Device, physical: vk::PhysicalDevice) -> Fsr4Support {
        let props = unsafe { device.physical_device_properties(physical) };
        let vendor_id = props.vendor_id;
        let driver_id = props.driver_id;

        // RDNA 4/5 (AMD) - full FSR 4 support with AI
        if vendor_id == 0x1002 {
            let name = unsafe {
                std::ffi::CStr::from_ptr(props.device_name.as_ptr())
                    .to_string_lossy().into_owned()
            };
            let name_lower = name.to_lowercase();
            if name_lower.contains("rdna 4") || name_lower.contains("rdna4") || 
               name_lower.contains("9000") || name_lower.contains("npu") {
                return Fsr4Support::Full;
            }
            // RDNA 3
            if name_lower.contains("rdna 3") || name_lower.contains("rdna3") {
                return Fsr4Support::Temporal;
            }
            return Fsr4Support::Temporal;
        }

        // Intel Arc - full support with XeSS 3 fallback
        if vendor_id == 0x8086 {
            return Fsr4Support::Temporal;
        }

        // Samsung Exynos - spatial + temporal
        if vendor_id == 0x1AE {
            return Fsr4Support::Temporal;
        }

        // Moore Threads - basic support
        if vendor_id == 0x1DD {
            return Fsr4Support::Spatial;
        }

        // Qualcomm/Adreno - temporal
        if vendor_id == 0x5143 {
            return Fsr4Support::Temporal;
        }

        // MediaTek - spatial
        // Huawei Kirin - spatial
        // Apple - temporal via MoltenVK
        Fsr4Support::Temporal
    }

    pub fn update(&mut self, quality: Fsr4Quality, mode: Fsr4Mode, ai_recon: bool, frame_gen: bool) {
        self.config.quality = quality as u32;
        self.config.mode = mode as u32;
        self.config.ai_reconstruction = ai_recon;
        self.config.frame_gen_enabled = frame_gen;
        self.is_ready = true;
    }
}
