//! DX12 descriptor heaps -- CBV, SRV, UAV, RTV, DSV, sampler (stub).

use crate::{Dx12Error, Dx12Result};

/// Descriptor heap category.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HeapType {
    CbvSrvUav,
    Sampler,
    Rtv,
    Dsv,
}

impl HeapType {
    /// D3D12_DESCRIPTOR_HEAP_TYPE_* value.
    pub fn d3d12_value(self) -> i32 {
        match self {
            Self::CbvSrvUav => 0,
            Self::Sampler => 1,
            Self::Rtv => 2,
            Self::Dsv => 3,
        }
    }
}

/// A descriptor heap. Stub.
#[derive(Debug)]
pub struct DescriptorHeap {
    pub heap_type: HeapType,
    pub capacity: u32,
    pub used: u32,
}

impl DescriptorHeap {
    /// Create a heap of the given capacity.
    pub fn new(heap_type: HeapType, capacity: u32) -> Dx12Result<Self> {
        if capacity == 0 {
            return Err(Dx12Error::InvalidParam("heap capacity must be > 0"));
        }
        Ok(Self { heap_type, capacity, used: 0 })
    }

    /// Allocate one descriptor slot (CPU handle space is stubbed).
    pub fn allocate(&mut self) -> Dx12Result<u32> {
        if self.used >= self.capacity {
            return Err(Dx12Error::InvalidParam("descriptor heap exhausted"));
        }
        let idx = self.used;
        self.used += 1;
        Ok(idx)
    }

    pub fn reset(&mut self) {
        self.used = 0;
    }
}
