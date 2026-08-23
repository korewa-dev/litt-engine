//! DXGI adapter enumeration (stub).
//!
//! Real implementation: `IDXGIFactory6::EnumAdapterByGpuPreference` and
//! `IDXGIAdapter4` capability queries via the `windows` crate.

use crate::{Dx12Backend, Dx12Error, Dx12Result};

/// One DXGI adapter (GPU).
#[derive(Clone, Debug)]
pub struct DxgiAdapterInfo {
    pub description: String,
    pub vendor_id: u32,
    pub device_id: u32,
    pub dedicated_video_memory_mb: u32,
    /// True when this is the Microsoft Basic Render Driver (WARP).
    pub software: bool,
}

/// DXGI instance / factory holder.
#[derive(Debug, Default)]
pub struct DxgiInstance {
    pub adapters: Vec<DxgiAdapterInfo>,
}

impl DxgiInstance {
    /// Create the factory and enumerate adapters.
    ///
    /// Stub: returns NotImplemented until the COM implementation lands.
    pub fn new() -> Dx12Result<Self> {
        Err(Dx12Error::NotImplemented("DXGI factory creation"))
    }

    /// Enumerate hardware + WARP adapters, best first.
    pub fn enumerate_adapters(&self) -> Dx12Result<Vec<DxgiAdapterInfo>> {
        Ok(self.adapters.clone())
    }

    /// Pick a backend kind for the given preference.
    pub fn select_backend(prefer_hardware: bool) -> Dx12Backend {
        if prefer_hardware {
            Dx12Backend::Hardware
        } else {
            Dx12Backend::Warp
        }
    }
}
