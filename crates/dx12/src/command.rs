//! DX12 command system -- queues, allocators, and command lists (stub).

use crate::{Dx12Error, Dx12Result};

/// Command queue type.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum QueueType {
    Direct,
    Compute,
    Copy,
}

impl QueueType {
    /// D3D12_COMMAND_LIST_TYPE_* value this maps to.
    pub fn d3d12_value(self) -> i32 {
        match self {
            Self::Direct => 0,
            Self::Compute => 2,
            Self::Copy => 3,
        }
    }
}

/// Command queue wrapper. Stub.
#[derive(Debug)]
pub struct CommandQueue {
    pub queue_type: QueueType,
}

impl CommandQueue {
    pub fn new(queue_type: QueueType) -> Self {
        Self { queue_type }
    }

    /// Execute recorded command lists.
    pub fn execute(&self, _lists: &[CommandList]) -> Dx12Result<()> {
        Err(Dx12Error::NotImplemented("ID3D12CommandQueue::ExecuteCommandLists"))
    }

    /// Signal a fence value (stub handle space).
    pub fn signal(&self) -> Dx12Result<u64> {
        Err(Dx12Error::NotImplemented("ID3D12Fence::Signal"))
    }
}

/// Graphics command list. Stub.
#[derive(Debug, Default)]
pub struct CommandList {
    pub is_open: bool,
}

impl CommandList {
    pub fn new() -> Self {
        Self { is_open: false }
    }

    pub fn begin(&mut self) -> Dx12Result<()> {
        self.is_open = true;
        Err(Dx12Error::NotImplemented("ID3D12GraphicsCommandList::Reset"))
    }

    pub fn end(&mut self) -> Dx12Result<()> {
        self.is_open = false;
        Err(Dx12Error::NotImplemented("ID3D12GraphicsCommandList::Close"))
    }

    /// Full GPU barrier (resource transition placeholder).
    pub fn barrier(&mut self) -> Dx12Result<()> {
        Err(Dx12Error::NotImplemented("ResourceBarrier"))
    }
}
