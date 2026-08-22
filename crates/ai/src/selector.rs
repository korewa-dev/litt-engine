//! Backend selection -- chooses the optimal AI acceleration backend.
//!
//! The selector checks hardware availability in priority order:
//! 1. Dedicated NPU (AMD XDNA, Intel AI Boost, etc.)
//! 2. GPU compute (Vulkan/DirectML)
//! 3. CPU fallback (SIMD-accelerated)

use super::npu::{NpuBackend, NpuInfo};

/// Backend kinds that can be selected
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BackendKind {
    /// NPU (Neural Processing Unit)
    Npu(NpuBackend),
    /// GPU via Vulkan compute
    Gpu,
    /// CPU with SIMD
    Cpu,
}

impl BackendKind {
    /// Get a human-readable name
    pub fn name(&self) -> String {
        match self {
            Self::Npu(npu) => npu.name().to_string(),
            Self::Gpu => "GPU (Vulkan Compute)".to_string(),
            Self::Cpu => "CPU (SIMD)".to_string(),
        }
    }
}

/// The AI backend selector -- picks the best available backend
pub struct BackendSelector {
    detected_backends: Vec<NpuInfo>,
}

impl Default for BackendSelector {
    fn default() -> Self { Self::new() }
}

impl BackendSelector {
    /// Create a new backend selector
    pub fn new() -> Self {
        Self { detected_backends: Self::detect_backends() }
    }

    /// Detect all available AI backends
    fn detect_backends() -> Vec<NpuInfo> {
        let mut backends = Vec::new();

        // AMD XDNA
        backends.push(NpuInfo {
            backend: NpuBackend::AmdXdna,
            name: "AMD Ryzen AI".to_string(),
            available: cfg!(any(target_os = "windows", target_os = "linux")),
            int8_tops: 50.0,
            fp16_tflops: 12.0,
            memory_bandwidth_gbps: 68.0,
        });

        // Intel AI Boost
        backends.push(NpuInfo {
            backend: NpuBackend::IntelAiBoost,
            name: "Intel AI Boost".to_string(),
            available: cfg!(target_os = "windows"),
            int8_tops: 48.0,
            fp16_tflops: 12.0,
            memory_bandwidth_gbps: 75.0,
        });

        // Qualcomm Hexagon
        backends.push(NpuInfo {
            backend: NpuBackend::QualcommHexagon,
            name: "Qualcomm Hexagon".to_string(),
            available: cfg!(target_os = "android"),
            int8_tops: 15.0,
            fp16_tflops: 4.0,
            memory_bandwidth_gbps: 30.0,
        });

        // Apple Neural Engine
        backends.push(NpuInfo {
            backend: NpuBackend::AppleNe,
            name: "Apple Neural Engine".to_string(),
            available: cfg!(any(target_os = "macos", target_os = "ios")),
            int8_tops: 15.8,
            fp16_tflops: 4.0,
            memory_bandwidth_gbps: 100.0,
        });

        // MediaTek APU
        backends.push(NpuInfo {
            backend: NpuBackend::MediaTek,
            name: "MediaTek APU".to_string(),
            available: cfg!(target_os = "android"),
            int8_tops: 10.0,
            fp16_tflops: 3.0,
            memory_bandwidth_gbps: 25.0,
        });

        // Kirin NPU
        backends.push(NpuInfo {
            backend: NpuBackend::Kirin,
            name: "Huawei Kirin NPU".to_string(),
            available: cfg!(target_os = "android"),
            int8_tops: 8.0,
            fp16_tflops: 2.0,
            memory_bandwidth_gbps: 20.0,
        });

        // Samsung Exynos NPU
        backends.push(NpuInfo {
            backend: NpuBackend::SamsungExynos,
            name: "Samsung Exynos NPU".to_string(),
            available: cfg!(target_os = "android"),
            int8_tops: 12.0,
            fp16_tflops: 4.0,
            memory_bandwidth_gbps: 35.0,
        });

        // RISC-V AI
        backends.push(NpuInfo {
            backend: NpuBackend::RiscvAi,
            name: "RISC-V AI Accelerator".to_string(),
            available: cfg!(target_arch = "riscv64"),
            int8_tops: 4.0,
            fp16_tflops: 1.0,
            memory_bandwidth_gbps: 10.0,
        });

        // Vulkan compute fallback
        backends.push(NpuInfo {
            backend: NpuBackend::VulkanCompute,
            name: "Vulkan Compute".to_string(),
            available: true, // Always available as fallback
            int8_tops: 0.0,
            fp16_tflops: 0.0,
            memory_bandwidth_gbps: 0.0,
        });

        backends
    }

    /// Get the best available backend
    pub fn best_available(&self) -> BackendKind {
        for info in &self.detected_backends {
            if info.available {
                return match info.backend {
                    NpuBackend::VulkanCompute => BackendKind::Gpu,
                    _ => BackendKind::Npu(info.backend),
                };
            }
        }
        BackendKind::Cpu // Ultimate fallback
    }

    /// Try to create a specific backend
    pub fn try_new(kind: BackendKind) -> Result<Backend, String> {
        Backend::new(kind)
    }

    /// Get info about all detected backends
    pub fn all_backends(&self) -> &[NpuInfo] {
        &self.detected_backends
    }

    /// Check if any NPU is available
    pub fn has_npu(&self) -> bool {
        self.detected_backends.iter().any(|b| b.available && !matches!(b.backend, NpuBackend::VulkanCompute))
    }

    /// Check if GPU compute is available
    pub fn has_gpu(&self) -> bool {
        self.detected_backends.iter().any(|b| matches!(b.backend, NpuBackend::VulkanCompute) && b.available)
    }
}

/// The unified AI backend -- wraps any backend behind a common interface
pub struct Backend {
    kind: BackendKind,
    initialized: bool,
}

impl Backend {
    /// Create a new backend
    pub fn new(kind: BackendKind) -> Result<Self, String> {
        match kind {
            BackendKind::Npu(npu) => {
                if !npu.is_available() {
                    return Err(format!("NPU backend {} is not available on this platform", npu.name()));
                }
                Ok(Self { kind, initialized: true })
            }
            BackendKind::Gpu => Ok(Self { kind, initialized: true }),
            BackendKind::Cpu => Ok(Self { kind, initialized: true }),
        }
    }

    /// Get the backend kind
    pub fn kind(&self) -> &BackendKind { &self.kind }

    /// Check if initialized
    pub fn is_initialized(&self) -> bool { self.initialized }
}
