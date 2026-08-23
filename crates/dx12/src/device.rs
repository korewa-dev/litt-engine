//! D3D12 logical device (stub).

use crate::instance::DxgiAdapterInfo;
use crate::{Dx12Error, Dx12Features, Dx12Result};

/// D3D12 device wrapper. Stub until the `windows`-based backend lands.
#[derive(Debug)]
pub struct D3D12Device {
    /// Adapter this device was created from.
    pub adapter: Option<DxgiAdapterInfo>,
    /// Feature bits reported by CheckFeatureSupport.
    pub features: Dx12Features,
}

impl D3D12Device {
    /// Create a device from an adapter.
    ///
    /// Stub: reports no features; every operation returns NotImplemented.
    pub fn new(adapter: &DxgiAdapterInfo) -> Dx12Result<Self> {
        Ok(Self {
            adapter: Some(adapter.clone()),
            features: Dx12Features::default(),
        })
    }

    /// Feature query placeholder.
    pub fn features(&self) -> Dx12Features {
        self.features
    }

    /// Wait for all GPU work (device-wide fence).
    pub fn wait_idle(&mut self) -> Dx12Result<()> {
        Err(Dx12Error::NotImplemented("ID3D12Device idle wait"))
    }
}
