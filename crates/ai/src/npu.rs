//! NPU backend types -- abstract representation for all NPU backends.
//! Each variant corresponds to a specific hardware NPU.

use super::tensor::DataType;

/// NPU backend variants
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NpuBackend {
    /// AMD Ryzen AI (XDNA architecture)
    AmdXdna,
    /// Intel AI Boost (Movidius/VPU)
    IntelAiBoost,
    /// Qualcomm Hexagon DSP
    QualcommHexagon,
    /// Apple Neural Engine
    AppleNe,
    /// MediaTek APU
    MediaTek,
    /// Huawei Kirin NPU (Da Vinci)
    Kirin,
    /// Samsung Exynos NPU
    SamsungExynos,
    /// RISC-V AI accelerator
    RiscvAi,
    /// Generic Vulkan compute (falls back to GPU)
    VulkanCompute,
}

impl NpuBackend {
    /// Check if this backend is available on the current system
    pub fn is_available(&self) -> bool {
        match self {
            Self::AmdXdna => cfg!(target_os = "windows") || cfg!(target_os = "linux"),
            Self::IntelAiBoost => cfg!(target_os = "windows"),
            Self::QualcommHexagon => cfg!(target_os = "android"),
            Self::AppleNe => cfg!(target_os = "macos") || cfg!(target_os = "ios"),
            Self::MediaTek => cfg!(target_os = "android"),
            Self::Kirin => cfg!(target_os = "android"),
            Self::SamsungExynos => cfg!(target_os = "android"),
            Self::RiscvAi => cfg!(target_arch = "riscv64"),
            Self::VulkanCompute => true, // Always available if Vulkan is
        }
    }

    /// Get a human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            Self::AmdXdna => "AMD Ryzen AI (XDNA)",
            Self::IntelAiBoost => "Intel AI Boost",
            Self::QualcommHexagon => "Qualcomm Hexagon DSP",
            Self::AppleNe => "Apple Neural Engine",
            Self::MediaTek => "MediaTek APU",
            Self::Kirin => "Huawei Kirin NPU",
            Self::SamsungExynos => "Samsung Exynos NPU",
            Self::RiscvAi => "RISC-V AI Accelerator",
            Self::VulkanCompute => "Vulkan Compute (Fallback)",
        }
    }

    /// Get supported precision modes
    pub fn supported_precisions(&self) -> Vec<DataType> {
        match self {
            Self::AmdXdna => vec![DataType::Float32, DataType::Float16, DataType::Int8],
            Self::IntelAiBoost => vec![DataType::Float32, DataType::Float16, DataType::Int8],
            Self::QualcommHexagon => vec![DataType::Float32, DataType::Float16, DataType::Int8, DataType::Uint8],
            Self::AppleNe => vec![DataType::Float32, DataType::Float16],
            Self::MediaTek => vec![DataType::Float32, DataType::Float16, DataType::Int8],
            Self::Kirin => vec![DataType::Float32, DataType::Int8],
            Self::SamsungExynos => vec![DataType::Float32, DataType::Float16, DataType::Int8],
            Self::RiscvAi => vec![DataType::Float32, DataType::Int8],
            Self::VulkanCompute => vec![DataType::Float32, DataType::Float16],
        }
    }
}

/// NPU info returned by detection
#[derive(Debug)]
pub struct NpuInfo {
    pub backend: NpuBackend,
    pub name: String,
    pub available: bool,
    pub int8_tops: f32,
    pub fp16_tflops: f32,
    pub memory_bandwidth_gbps: f32,
}

impl NpuInfo {
    pub fn unavailable(backend: NpuBackend, name: String) -> Self {
        Self {
            backend,
            name,
            available: false,
            int8_tops: 0.0,
            fp16_tflops: 0.0,
            memory_bandwidth_gbps: 0.0,
        }
    }
}
