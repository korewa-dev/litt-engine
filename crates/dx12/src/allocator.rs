//! DX12 resource allocation -- heaps, buffers, textures (stub).

use crate::{Dx12Error, Dx12Result};

/// Memory residency class.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HeapKind {
    /// GPU-only (DEFAULT heap).
    DeviceLocal,
    /// CPU-write, GPU-read every frame (UPLOAD heap).
    Upload,
    /// CPU-visible readback (READBACK heap).
    Readback,
}

impl HeapKind {
    /// D3D12_HEAP_TYPE_* value.
    pub fn d3d12_value(self) -> i32 {
        match self {
            Self::DeviceLocal => 1,
            Self::Upload => 2,
            Self::Readback => 3,
        }
    }
}

/// A committed resource allocation. Stub.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct GpuAllocation {
    pub size: u64,
    pub kind: HeapKind,
    pub handle: u64,
}

/// Resource allocator. Stub.
#[derive(Debug, Default)]
pub struct D3D12Allocator {
    next_handle: u64,
    pub live_allocations: usize,
    pub allocated_bytes: u64,
}

impl D3D12Allocator {
    pub fn new() -> Self {
        Self::default()
    }

    /// Allocate a buffer on a heap.
    pub fn allocate_buffer(&mut self, size: u64, _kind: HeapKind) -> Dx12Result<GpuAllocation> {
        if size == 0 {
            return Err(Dx12Error::InvalidParam("buffer size must be > 0"));
        }
        self.next_handle += 1;
        Err(Dx12Error::NotImplemented("CreateCommittedResource"))
    }

    /// Free a previous allocation.
    pub fn free(&mut self, _allocation: GpuAllocation) -> Dx12Result<()> {
        Err(Dx12Error::NotImplemented("Resource release"))
    }

    /// Live allocation count.
    pub fn live_allocations(&self) -> usize {
        self.live_allocations
    }
}
