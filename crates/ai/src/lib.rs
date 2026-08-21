//! Universal AI Acceleration Layer for Litt Engine
//!
//! Provides a unified inference interface across multiple hardware backends:
//! - **AMD XDNA** — Ryzen AI NPU via Vulkan compute shaders
//! - **Intel AI Boost** — Movidius/VPU via OpenVINO or DirectML
//! - **Qualcomm Hexagon** — DSP via NNAPI (Android) or Hexagon SDK
//! - **Apple Neural Engine** — Core ML (macOS/iOS)
//! - **MediaTek APU** — Vendor SDK
//! - **Kirin NPU** — Huawei Da Vinci architecture
//! - **Samsung Exynos NPU** — ARM Mali + NPU
//! - **RISC-V AI** — Custom vector accelerators
//! - **CPU** — Fallback with SIMD (AVX2/NEON/RVV)
//!
//! # Usage
//! ```rust
//! use litt_ai::*;
//!
//! // Auto-select best backend
//! let selector = BackendSelector::new();
//! let backend = selector.best_available();
//!
//! // Or force a specific backend
//! let backend = Backend::try_new(BackendKind::Npu(NpuBackend::AmdXdna))?;
//!
//! // Run inference
//! let output = backend.run(&model, &inputs)?;
//! ```

pub mod backend;
pub mod selector;
pub mod tensor;
pub mod model;
pub mod execution;
pub mod npu;

pub use backend::*;
pub use selector::*;
pub use tensor::*;
pub use model::*;
pub use execution::*;
pub use npu::*;

// ECS integration re-exports
pub use litt_ecs::*;
