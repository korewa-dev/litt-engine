//! NPU vendor detection and integration for the fidelityfx crate.
//! Bridges the litt-ai backend selector with the rendering pipeline.

use litt_math::Vec3;

/// NPU vendor types (shared with litt-ai)
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum NpuVendor {
    #[default]
    Unknown,
    AmdXdna,
    IntelAiBoost,
    QualcommHexagon,
    AppleNe,
    MediaTek,
    Kirin,
    SamsungRdna,
    RiscvAi,
    Other(String),
}

/// NPU capabilities (simplified)
#[derive(Clone, Copy, Debug, Default)]
pub struct NpuCapabilities {
    pub mm_count: u32,
    pub fp16_tflops: f32,
    pub int8_tops: f32,
    pub bandwidth_gbps: f32,
    pub precision_mask: u32,
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

/// Query NPU info from Vulkan device properties
pub fn detect_npu_vendor(device_name: &str, vendor_id: u32) -> NpuVendor {
    let n = device_name.to_lowercase();
    match vendor_id {
        0x1002 if n.contains("ryzen") || n.contains("ai") || n.contains("xdna") => NpuVendor::AmdXdna,
        0x8086 if n.contains("ai") || n.contains("boost") || n.contains("core") => NpuVendor::IntelAiBoost,
        0x5143 | _ if n.contains("qualcomm") || n.contains("hexagon") || n.contains("adreno") => NpuVendor::QualcommHexagon,
        _ if n.contains("mediatek") => NpuVendor::MediaTek,
        _ if n.contains("kirin") || n.contains("hisi") => NpuVendor::Kirin,
        _ if n.contains("samsung") || vendor_id == 0x1AE => NpuVendor::SamsungRdna,
        _ if n.contains("riscv") || n.contains("risc-v") => NpuVendor::RiscvAi,
        _ => NpuVendor::Other(device_name.to_string()),
    }
}

/// Estimate NPU capabilities based on vendor
pub fn estimate_npu_capabilities(vendor: NpuVendor, device_name: &str) -> NpuCapabilities {
    let n = device_name.to_lowercase();
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
        NpuVendor::SamsungRdna => NpuCapabilities { mm_count: 64, fp16_tflops: 4.0, int8_tops: 12.0, bandwidth_gbps: 35.0, precision_mask: 0b0011 },
        NpuVendor::RiscvAi => NpuCapabilities { mm_count: 16, fp16_tflops: 1.0, int8_tops: 4.0, bandwidth_gbps: 10.0, precision_mask: 0b0001 },
        _ => NpuCapabilities { mm_count: 32, fp16_tflops: 2.0, int8_tops: 8.0, bandwidth_gbps: 20.0, precision_mask: 0b0001 },
    }
}

/// Check if NPU acceleration is available
pub fn npu_is_available(vendor: NpuVendor) -> bool {
    match vendor {
        NpuVendor::AmdXdna => true,
        NpuVendor::IntelAiBoost => true,
        NpuVendor::QualcommHexagon => cfg!(target_os = "android"),
        NpuVendor::AppleNe => cfg!(any(target_os = "macos", target_os = "ios")),
        NpuVendor::MediaTek | NpuVendor::Kirin | NpuVendor::SamsungRdna => cfg!(target_os = "android"),
        NpuVendor::RiscvAi => cfg!(target_arch = "riscv64"),
        _ => false,
    }
}
