//! DX12 Backend - DirectX 12 with Ray Tracing (DXR)
//!
//! STATUS: interface-complete stub.
//!
//! The full COM implementation (DXGI factory enumeration, D3D12 device,
//! command queues, descriptor heaps, PSOs, DXR) requires the `windows`
//! crate bindings. It was previously written against hallucinated winapi
//! APIs and could not compile; it is now an honest stub so the workspace
//! builds and the backend can attach through [`litt_gal`] when finished.
//!
//! Implementation checklist (in order):
//! 1. `windows` crate with features Win32_Graphics_Direct3D12,
//!    Win32_Graphics_Dxgi, Win32_Graphics_Direct3D, Win32_System_Threading.
//! 2. DXGI factory + adapter enum (hardware / WARP).
//! 3. ID3D12Device creation with feature checks (DXR 1.0/1.1).
//! 4. Direct/Compute/Copy command queues + allocators.
//! 5. CBV/SRV/UAV + RTV/DSV descriptor heaps.
//! 6. Graphics + compute PSO compilation from DXIL.
//! 7. DXR: BLAS/TLAS build + shader binding table.
//! 8. Swapchain (flip-model, tearing for frame generation).

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(clippy::result_large_err)]

use std::fmt;

pub mod instance;
pub mod device;
pub mod swapchain;
pub mod command;
pub mod descriptor;
pub mod pipeline;
pub mod ray_tracing;
pub mod shader;
pub mod allocator;

pub use instance::*;
pub use device::*;
pub use swapchain::*;
pub use command::*;
pub use descriptor::*;
pub use pipeline::*;
pub use ray_tracing::*;
pub use shader::*;
pub use allocator::*;

/// Error returned by every DX12 operation until the real backend lands.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Dx12Error {
    /// Operation would succeed once the COM implementation exists.
    NotImplemented(&'static str),
    /// Invalid parameter detected before reaching any API call.
    InvalidParam(&'static str),
}

impl fmt::Display for Dx12Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Dx12Error::NotImplemented(op) => {
                write!(f, "DX12 backend not implemented yet: {op}")
            }
            Dx12Error::InvalidParam(what) => write!(f, "invalid parameter: {what}"),
        }
    }
}

impl std::error::Error for Dx12Error {}

/// Convenience result alias.
pub type Dx12Result<T> = Result<T, Dx12Error>;

/// DX12 backend feature flags
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Dx12Features {
    pub ray_tracing: bool,
    pub mesh_shader: bool,
    pub variable_rate_shading: bool,
    pub samplers_on_heap: bool,
    pub typed_uav_loads: bool,
}

/// Backend selection result
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Dx12Backend {
    Hardware,
    Warp,
    Null,
}

impl Default for Dx12Backend {
    fn default() -> Self {
        Self::Hardware
    }
}

impl Dx12Backend {
    /// Human-readable name.
    pub fn name(&self) -> &'static str {
        match self {
            Self::Hardware => "D3D12 Hardware",
            Self::Warp => "D3D12 WARP (software)",
            Self::Null => "D3D12 Null",
        }
    }
}
