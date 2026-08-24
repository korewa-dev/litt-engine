//! Execution context -- manages model loading, tensor allocation, and inference dispatch.

use super::model::Model;
use super::tensor::{InferenceResult, Tensor};
use super::selector::{BackendKind, BackendSelector};
use super::backend::{AIBackend, amd_xdna, intel_ai, hexagon, core_ml, cpu, vulkan_compute};

/// The main AI execution context
#[derive(Debug)]
pub struct AIContext {
    selector: BackendSelector,
    active_backend: Option<BackendWrapper>,
}

/// Wrapper for any backend implementation
#[derive(Debug)]
pub enum BackendWrapper {
    AmdXdna(amd_xdna::AmdXdnaBackend),
    IntelAi(intel_ai::IntelAiBackend),
    Hexagon(hexagon::HexagonBackend),
    CoreML(core_ml::CoreMLBackend),
    Cpu(cpu::CpuBackend),
    VulkanCompute(vulkan_compute::VulkanComputeBackend),
}

impl BackendWrapper {
    pub fn run(&mut self, model: &Model, inputs: &[Tensor]) -> Result<InferenceResult, String> {
        match self {
            Self::AmdXdna(b) => b.run(model, inputs),
            Self::IntelAi(b) => b.run(model, inputs),
            Self::Hexagon(b) => b.run(model, inputs),
            Self::CoreML(b) => b.run(model, inputs),
            Self::Cpu(b) => b.run(model, inputs),
            Self::VulkanCompute(b) => b.run(model, inputs),
        }
    }

    pub fn kind(&self) -> BackendKind {
        match self {
            Self::AmdXdna(b) => b.kind(),
            Self::IntelAi(b) => b.kind(),
            Self::Hexagon(b) => b.kind(),
            Self::CoreML(b) => b.kind(),
            Self::Cpu(b) => b.kind(),
            Self::VulkanCompute(b) => b.kind(),
        }
    }
}

impl AIContext {
    /// Create a new AI context with automatic backend selection
    pub fn new() -> Self {
        Self {
            selector: BackendSelector::new(),
            active_backend: None,
        }
    }

    /// Get the best available backend kind
    pub fn best_backend_kind(&self) -> BackendKind {
        self.selector.best_available()
    }

    /// Initialize the best available backend
    pub fn init_best(&mut self) -> Result<(), String> {
        let kind = self.selector.best_available();
        self.init_backend(kind)
    }

    /// Initialize a specific backend
    pub fn init_backend(&mut self, kind: BackendKind) -> Result<(), String> {
        let wrapper = match kind {
            BackendKind::Npu(super::npu::NpuBackend::AmdXdna) => {
                BackendWrapper::AmdXdna({
                    let mut b = amd_xdna::AmdXdnaBackend::new(0);
                    b.ready = true;
                    b
                })
            }
            BackendKind::Npu(super::npu::NpuBackend::IntelAiBoost) => {
                BackendWrapper::IntelAi({
                    let mut b = intel_ai::IntelAiBackend::new();
                    b.ready = true;
                    b
                })
            }
            BackendKind::Npu(super::npu::NpuBackend::QualcommHexagon) => {
                BackendWrapper::Hexagon({
                    let mut b = hexagon::HexagonBackend::new();
                    b.ready = true;
                    b
                })
            }
            BackendKind::Npu(super::npu::NpuBackend::AppleNe) => {
                BackendWrapper::CoreML({
                    let mut b = core_ml::CoreMLBackend::new();
                    b.ready = true;
                    b
                })
            }
            BackendKind::Gpu => {
                BackendWrapper::VulkanCompute({
                    let mut b = vulkan_compute::VulkanComputeBackend::new();
                    b.ready = true;
                    b
                })
            }
            BackendKind::Cpu => {
                BackendWrapper::Cpu(cpu::CpuBackend::new())
            }
            _ => return Err(format!("Unsupported backend kind: {kind:?}")),
        };

        self.active_backend = Some(wrapper);
        Ok(())
    }

    /// Run inference with the active backend
    pub fn run(&mut self, model: &Model, inputs: &[Tensor]) -> Result<InferenceResult, String> {
        if self.active_backend.is_none() {
            self.init_best()?;
        }

        match &mut self.active_backend {
            Some(wrapper) => wrapper.run(model, inputs),
            None => Err("No backend initialized".to_string()),
        }
    }

    /// Run inference, auto-selecting the best backend if needed
    pub fn run_auto(&mut self, model: &Model, inputs: &[Tensor]) -> Result<InferenceResult, String> {
        // Try NPU first, then GPU, then CPU
        if self.init_backend(BackendKind::Npu(super::npu::NpuBackend::AmdXdna)).is_ok() {
            if let Ok(result) = self.run(model, inputs) {
                return Ok(result);
            }
        }
        if self.init_backend(BackendKind::Npu(super::npu::NpuBackend::IntelAiBoost)).is_ok() {
            if let Ok(result) = self.run(model, inputs) {
                return Ok(result);
            }
        }
        if self.init_backend(BackendKind::Gpu).is_ok() {
            if let Ok(result) = self.run(model, inputs) {
                return Ok(result);
            }
        }
        if self.init_backend(BackendKind::Cpu).is_ok() {
            return self.run(model, inputs);
        }

        Err("No suitable backend available".to_string())
    }

    /// Get info about all detected backends
    pub fn backend_info(&self) -> &[super::npu::NpuInfo] {
        self.selector.all_backends()
    }

    /// Check if any NPU is available
    pub fn has_npu(&self) -> bool {
        self.selector.has_npu()
    }

    /// Check if GPU compute is available
    pub fn has_gpu(&self) -> bool {
        self.selector.has_gpu()
    }
}

impl Default for AIContext {
    fn default() -> Self { Self::new() }
}
