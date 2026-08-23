//! DX12 pipeline state objects (PSOs) -- stub.

use crate::{Dx12Error, Dx12Result};

/// Opaque PSO handle (real impl wraps ID3D12PipelineState).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PipelineState(pub u64);

/// Graphics/compute PSO creation. Stub.
#[derive(Debug, Default)]
pub struct PipelineStateManager {
    next_handle: u64,
    pub pipelines: Vec<PipelineState>,
}

impl PipelineStateManager {
    pub fn new() -> Self {
        Self::default()
    }

    /// Create a compute PSO from DXIL bytecode.
    pub fn create_compute(&mut self, _dxil: &[u8], _entry: &str) -> Dx12Result<PipelineState> {
        self.next_handle += 1;
        let handle = PipelineState(self.next_handle);
        Err(Dx12Error::NotImplemented("D3D12ComputePipelineState creation"))
    }

    /// Create a graphics PSO from DXIL bytecode.
    pub fn create_graphics(&mut self, _dxil_vs: &[u8], _dxil_ps: &[u8]) -> Dx12Result<PipelineState> {
        self.next_handle += 1;
        Err(Dx12Error::NotImplemented("D3D12GraphicsPipelineState creation"))
    }

    pub fn len(&self) -> usize {
        self.pipelines.len()
    }

    pub fn is_empty(&self) -> bool {
        self.pipelines.is_empty()
    }
}
