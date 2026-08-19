//! AMD FSR 4 Integration (ML-Upscaler 4.1.1 + ML Frame Gen 4.0.1)
//! Based on official AMD FidelityFX SDK 2.3.0 "Redstone"
//!
//! Note: Official SDK is DX12-only. This module provides Vulkan compatibility
//! through OptiScaler interop or direct implementation.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// FSR 4 version info
pub const FSR4_VERSION: &str = "4.1.1";
pub const FSR4_FRAMEGEN_VERSION: &str = "4.0.1";
pub const FSR_SDK_VERSION: &str = "2.3.0";

/// FSR 4 quality presets (matches SDK)
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Quality {
    #[default]
    UltraQuality,    // 0.56x - 4K to 2.24K
    Quality,         // 0.67x - 4K to 2.67K  
    Balanced,        // 0.83x - 4K to 3.33K
    Performance,     // 1.0x  - native
    UltraPerformance, // 1.5x - oversampled
}

/// FSR 4 mode
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Mode {
    #[default]
    Upscale,          // ML upscaling only
    FrameGeneration,  // ML frame generation
    FullPipeline,     // Upscale + Frame Gen
}

/// FSR 4 configuration (SDK compatible)
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr4Desc {
    /// Input texture dimensions
    pub src_width: u32,
    pub src_height: u32,
    /// Output texture dimensions  
    pub dst_width: u32,
    pub dst_height: u32,
    /// Quality preset
    pub quality: u32,
    /// Mode (upscale/framegen/full)
    pub mode: u32,
    /// Sharpness (0.0 - 1.0)
    pub sharpness: f32,
    /// Temporal stability (0.0 - 1.0)
    pub temporal_stability: f32,
    /// AI reconstruction enabled
    pub ai_reconstruction: bool,
    /// Frame generation enabled
    pub frame_generation: bool,
    pub _pad: [u32; 4],
}

impl Default for Fsr4Desc {
    fn default() -> Self {
        Self {
            src_width: 0,
            src_height: 0,
            dst_width: 0,
            dst_height: 0,
            quality: 0,
            mode: 0,
            sharpness: 0.25,
            temporal_stability: 0.5,
            ai_reconstruction: false,
            frame_generation: false,
            _pad: [0; 4],
        }
    }
}

/// FSR 4 pipeline state
#[derive(Debug)]
pub struct Fsr4Pipeline {
    pub desc: Fsr4Desc,
    pub is_ready: bool,
    pub support_level: Fsr4Support,
    pub vendor: String,
}

/// GPU support level for FSR 4 ML features
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum Fsr4Support {
    #[default]
    None,
    /// FSR 3.1.5 temporal (no ML)
    Temporal,
    /// FSR 4 ML upscaling available
    UpscaleML,
    /// Full FSR 4 with frame generation
    Full,
}

impl Fsr4Pipeline {
    /// Create new FSR 4 pipeline
    pub fn new(src_w: u32, src_h: u32, dst_w: u32, dst_h: u32) -> Self {
        Self {
            desc: Fsr4Desc {
                src_width: src_w,
                src_height: src_h,
                dst_width: dst_w,
                dst_height: dst_h,
                ..Default::default()
            },
            is_ready: false,
            support_level: Fsr4Support::Temporal,
            vendor: String::new(),
        }
    }

    /// Detect GPU support for FSR 4
    pub fn detect_support(device: &Device, physical: vk::PhysicalDevice) -> Fsr4Support {
        use litt_vulkan::GpuVendor;
        
        let props = unsafe { device.physical_device_properties(physical) };
        let vendor_id = props.vendor_id;
        let name = unsafe {
            std::ffi::CStr::from_ptr(props.device_name.as_ptr())
                .to_string_lossy().into_owned()
        };
        
        // Determine vendor
        let vendor = match vendor_id {
            0x1002 => GpuVendor::Amd,
            0x8086 => GpuVendor::Intel,
            0x1AE => GpuVendor::Samsung,
            0x1DD => GpuVendor::MooreThreads,
            0x5143 => GpuVendor::Other("Qualcomm".to_string()),
            _ => GpuVendor::Other(name.clone()),
        };
        
        // FSR 4 ML support
        match vendor {
            GpuVendor::Amd => {
                // RDNA 3/4 desktop = full FSR 4
                if name.to_lowercase().contains("rdna 3") || 
                   name.to_lowercase().contains("rdna3") ||
                   name.to_lowercase().contains("rdna 4") ||
                   name.to_lowercase().contains("rdna4") ||
                   name.to_lowercase().contains("7") ||
                   name.to_lowercase().contains("9") {
                    Fsr4Support::Full
                } else if name.to_lowercase().contains("rdna 2") || name.to_lowercase().contains("rdna2") {
                    Fsr4Support::Temporal  // FSR 4.1 coming 2027
                } else {
                    Fsr4Support::Temporal
                }
            }
            GpuVendor::Intel | GpuVendor::Samsung => Fsr4Support::UpscaleML,
            GpuVendor::MooreThreads | GpuVendor::Other(_) => Fsr4Support::Temporal,
            _ => Fsr4Support::Temporal,
        }
    }

    /// Configure pipeline for upscaling only
    pub fn configure_upscale(&mut self, quality: Fsr4Quality, ai_recon: bool) {
        self.desc.quality = quality as u32;
        self.desc.mode = 0; // Upscale
        self.desc.ai_reconstruction = ai_recon;
        self.desc.frame_generation = false;
        self.is_ready = true;
    }

    /// Configure pipeline for frame generation
    pub fn configure_framegen(&mut self, quality: Fsr4Quality) {
        self.desc.quality = quality as u32;
        self.desc.mode = 1; // FrameGen
        self.desc.ai_reconstruction = false;
        self.desc.frame_generation = true;
        self.is_ready = true;
    }

    /// Get recommended quality for current support level
    pub fn recommended_quality(&self) -> Fsr4Quality {
        match self.support_level {
            Fsr4Support::Full => Fsr4Quality::Quality,
            Fsr4Support::UpscaleML => Fsr4Quality::Balanced,
            _ => Fsr4Quality::Performance,
        }
    }
}

/// FSR 4 shader constants for compute shader
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct Fsr4Constants {
    pub src_width: u32,
    pub src_height: u32,
    pub dst_width: u32,
    pub dst_height: u32,
    pub quality: u32,
    pub mode: u32,
    pub sharpness: f32,
    pub temporal_stability: f32,
    pub ai_reconstruction: u32,
    pub frame_generation: u32,
    pub _pad: [u32; 4],
}

impl From<&Fsr4Desc> for Fsr4Constants {
    fn from(desc: &Fsr4Desc) -> Self {
        Self {
            src_width: desc.src_width,
            src_height: desc.src_height,
            dst_width: desc.dst_width,
            dst_height: desc.dst_height,
            quality: desc.quality,
            mode: desc.mode,
            sharpness: desc.sharpness,
            temporal_stability: desc.temporal_stability,
            ai_reconstruction: if desc.ai_reconstruction { 1 } else { 0 },
            frame_generation: if desc.frame_generation { 1 } else { 0 },
            _pad: [0; 4],
        }
    }
}
