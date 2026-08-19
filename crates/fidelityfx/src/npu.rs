//! NPU (Neural Processing Unit) acceleration support.
//! Targets: Ryzen AI (XDNA), Intel AI Boost (Movidius), mobile NPUs (Qualcomm, Apple).

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use litt_math::*;

/// NPU vendor types
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum NpuVendor {
    #[default]
    Unknown,
    /// AMD Ryzen AI (XDNA architecture)
    AmdXdna,
    /// Intel AI Boost (Movidius/VPU)
    IntelAiBoost,
    /// Qualcomm Hexagon (mobile)
    QualcommHexagon,
    /// Apple Neural Engine
    AppleNe,
    /// MediaTek APU
    MediaTek,
    /// Samsung NPUs
    Samsung,
    /// Other
    Other(String),
}

/// NPU capabilities queryable via Vulkan
#[derive(Clone, Copy, Debug, Default, Pod, Zeroable)]
#[repr(C)]
pub struct NpuCapabilities {
    /// Max matrix multiply units
    pub mm_count: u32,
    /// Max FP16 throughput (TFLOPS)
    pub fp16_tflops: f32,
    /// Max INT8 throughput (TOPS)
    pub int8_tops: f32,
    /// Max memory bandwidth (GB/s)
    pub bandwidth_gbps: f32,
    /// Supported precision modes
    pub precision_mask: u32, // bit 0=FP16, bit 1=INT8, bit 2=INT4, bit 3=BF16
}

/// NPU acceleration mode
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum NpuMode {
    #[default]
    Disabled,
    /// Auto: use NPU when available and beneficial
    Auto,
    /// Force NPU for all denoising tasks
    Forced,
    /// Hybrid: NPU for denoise, GPU for ray tracing
    Hybrid,
}

/// NPU configuration
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct NpuConfig {
    pub mode: u32,          // NpuMode as u32
    pub vendor: u32,        // NpuVendor as u32
    pub precision: u32,     // preferred precision mask
    pub max_latency_ms: f32,
    pub fallback_to_gpu: bool,
    pub _pad: [u32; 3],
}

impl NpuConfig {
    pub fn new() -> Self {
        Self {
            mode: 0, // Disabled
            vendor: 0,
            precision: 0b0001, // FP16
            max_latency_ms: 8.0,
            fallback_to_gpu: true,
            _pad: [0; 3],
        }
    }
}

/// Detected NPU info
#[derive(Debug)]
pub struct NpuInfo {
    pub vendor: NpuVendor,
    pub name: String,
    pub capabilities: NpuCapabilities,
    pub available: bool,
}

impl NpuInfo {
    pub fn disabled() -> Self {
        Self {
            vendor: NpuVendor::Unknown,
            name: String::new(),
            capabilities: NpuCapabilities::default(),
            available: false,
        }
    }
}

/// Query available NPUs via Vulkan physical device properties
pub fn detect_npus(device: &Device, physical: vk::PhysicalDevice) -> Vec<NpuInfo> {
    let mut npus = Vec::new();
    
    // Query device name
    let props = unsafe { device.physical_device_properties(physical) };
    let name = unsafe {
        std::ffi::CStr::from_ptr(props.device_name.as_ptr())
            .to_string_lossy().into_owned()
    };
    
    // Detect vendor from device name and vendor ID
    let vendor_id = props.vendor_id;
    let vendor = match vendor_id {
        0x1002 => NpuVendor::AmdXdna,
        0x8086 => NpuVendor::IntelAiBoost,
        0x10DE => NpuVendor::Other("NVIDIA (no NPU)".to_string()),
        _ => NpuVendor::Other(name.clone()),
    };
    
    // Check for NPU compute capability
    let queue_props = unsafe { device.physical_device_queue_family_properties(physical) };
    let has_npu_queue = queue_props.iter().any(|q| {
        q.queue_flags.contains(vk::QueueFlags::COMPUTE) 
        && q.queue_count > 0
    });
    
    // Estimate capabilities based on device name patterns
    let caps = estimate_npu_capabilities(&name, vendor_id);
    
    npus.push(NpuInfo {
        vendor,
        name,
        capabilities: caps,
        available: has_npu_queue,
    });
    
    npus
}

/// Estimate NPU capabilities from device name
fn estimate_npu_capabilities(name: &str, vendor_id: u32) -> NpuCapabilities {
    let name_lower = name.to_lowercase();
    
    // AMD Ryzen AI (XDNA)
    if name_lower.contains("ryzen") || name_lower.contains("xdna") || name_lower.contains("ai") {
        if name_lower.contains("9") || name_lower.contains("h") {
            return NpuCapabilities {
                mm_count: 256,
                fp16_tflops: 12.0,
                int8_tops: 50.0,
                bandwidth_gbps: 68.0,
                precision_mask: 0b0111, // FP16 + INT8 + BF16
            };
        }
        return NpuCapabilities {
            mm_count: 128,
            fp16_tflops: 6.0,
            int8_tops: 25.0,
            bandwidth_gbps: 48.0,
            precision_mask: 0b0111,
        };
    }
    
    // Intel AI Boost
    if name_lower.contains("intel") || name_lower.contains("arc") {
        if name_lower.contains("core") && (name_lower.contains("ultra") || name_lower.contains("h")) {
            return NpuCapabilities {
                mm_count: 128,
                fp16_tflops: 12.0,
                int8_tops: 48.0,
                bandwidth_gbps: 75.0,
                precision_mask: 0b0111,
            };
        }
        return NpuCapabilities {
            mm_count: 64,
            fp16_tflops: 6.0,
            int8_tops: 24.0,
            bandwidth_gbps: 45.0,
            precision_mask: 0b0011,
        };
    }
    
    // Qualcomm (mobile)
    if vendor_id == 0x5143 || name_lower.contains("qualcomm") || name_lower.contains("adreno") {
        return NpuCapabilities {
            mm_count: 64,
            fp16_tflops: 4.0,
            int8_tops: 15.0,
            bandwidth_gbps: 30.0,
            precision_mask: 0b0011,
        };
    }
    
    // Default
    NpuCapabilities {
        mm_count: 32,
        fp16_tflops: 2.0,
        int8_tops: 8.0,
        bandwidth_gbps: 20.0,
        precision_mask: 0b0001,
    }
}
