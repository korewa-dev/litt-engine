//! NPU (Neural Processing Unit) acceleration support.
//! Targets: Ryzen AI (XDNA), Intel AI Boost, mobile NPUs (Qualcomm, MediaTek, Kirin).

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
    /// Huawei Kirin NPU
    Kirin,
    /// Samsung Exynos with AMD RDNA iGPU (Exynos 2200+)
    SamsungRdna,
    /// RISC-V AI accelerators
    RiscvAi,
    /// Other
    Other(String),
}

/// NPU capabilities
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
    Auto,
    Forced,
    Hybrid,
}

/// NPU configuration
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct NpuConfig {
    pub mode: u32,
    pub vendor: u32,
    pub precision: u32,
    pub max_latency_ms: f32,
    pub fallback_to_gpu: bool,
    pub _pad: [u32; 3],
}

impl NpuConfig {
    pub fn new() -> Self {
        Self {
            mode: 0,
            vendor: 0,
            precision: 0b0001,
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

/// Query available NPUs
pub fn detect_npus(device: &Device, physical: vk::PhysicalDevice) -> Vec<NpuInfo> {
    let mut npus = Vec::new();
    let props = unsafe { device.physical_device_properties(physical) };
    let name = unsafe {
        std::ffi::CStr::from_ptr(props.device_name.as_ptr())
            .to_string_lossy().into_owned()
    };
    let vendor_id = props.vendor_id;
    let vendor = detect_npu_vendor(&name, vendor_id);
    let queue_props = unsafe { device.physical_device_queue_family_properties(physical) };
    let has_npu_queue = queue_props.iter().any(|q| q.queue_flags.contains(vk::QueueFlags::COMPUTE));
    let caps = estimate_npu_capabilities(&name, vendor_id, vendor);
    npus.push(NpuInfo { vendor, name, capabilities: caps, available: has_npu_queue });
    npus
}

fn detect_npu_vendor(name: &str, vendor_id: u32) -> NpuVendor {
    let n = name.to_lowercase();
    match vendor_id {
        0x1002 if n.contains("ryzen") || n.contains("ai") || n.contains("xdna") => NpuVendor::AmdXdna,
        0x8086 if n.contains("ai") || n.contains("boost") || n.contains("core") => NpuVendor::IntelAiBoost,
        0x5143 | _ if n.contains("qualcomm") || n.contains("hexagon") || n.contains("adreno") => NpuVendor::QualcommHexagon,
        _ if n.contains("mediaTek") || n.contains("mediatek") => NpuVendor::MediaTek,
        _ if n.contains("kirin") || n.contains("hiSilicon") => NpuVendor::Kirin,
        _ if n.contains("samsung") || vendor_id == 0x1AE => NpuVendor::SamsungRdna,
        _ if n.contains("riscv") || n.contains("risc-v") => NpuVendor::RiscvAi,
        _ => NpuVendor::Other(name.clone()),
    }
}

fn estimate_npu_capabilities(name: &str, vendor_id: u32, vendor: NpuVendor) -> NpuCapabilities {
    let n = name.to_lowercase();
    match vendor {
        NpuVendor::AmdXdna => {
            if n.contains("9") || n.contains("h") {
                NpuCapabilities { mm_count: 256, fp16_tflops: 12.0, int8_tops: 50.0, bandwidth_gbps: 68.0, precision_mask: 0b0111 }
            } else {
                NpuCapabilities { mm_count: 128, fp16_tflops: 6.0, int8_tops: 25.0, bandwidth_gbps: 48.0, precision_mask: 0b0111 }
            }
        }
        NpuVendor::IntelAiBoost => {
            if n.contains("ultra") || n.contains("h") {
                NpuCapabilities { mm_count: 128, fp16_tflops: 12.0, int8_tops: 48.0, bandwidth_gbps: 75.0, precision_mask: 0b0111 }
            } else {
                NpuCapabilities { mm_count: 64, fp16_tflops: 6.0, int8_tops: 24.0, bandwidth_gbps: 45.0, precision_mask: 0b0011 }
            }
        }
        NpuVendor::QualcommHexagon => NpuCapabilities { mm_count: 64, fp16_tflops: 4.0, int8_tops: 15.0, bandwidth_gbps: 30.0, precision_mask: 0b0011 },
        NpuVendor::MediaTek => NpuCapabilities { mm_count: 48, fp16_tflops: 3.0, int8_tops: 10.0, bandwidth_gbps: 25.0, precision_mask: 0b0011 },
        NpuVendor::Kirin => NpuCapabilities { mm_count: 32, fp16_tflops: 2.0, int8_tops: 8.0, bandwidth_gbps: 20.0, precision_mask: 0b0001 },
        NpuVendor::SamsungRdna => {
            // Exynos 2200+ has AMD RDNA 2 iGPU with integrated NPU
            NpuCapabilities { mm_count: 64, fp16_tflops: 4.0, int8_tops: 12.0, bandwidth_gbps: 35.0, precision_mask: 0b0011 }
        }
        NpuVendor::RiscvAi => NpuCapabilities { mm_count: 16, fp16_tflops: 1.0, int8_tops: 4.0, bandwidth_gbps: 10.0, precision_mask: 0b0001 },
        _ => NpuCapabilities { mm_count: 32, fp16_tflops: 2.0, int8_tops: 8.0, bandwidth_gbps: 20.0, precision_mask: 0b0001 },
    }
}
