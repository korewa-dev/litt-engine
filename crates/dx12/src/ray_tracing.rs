//! DXR ray tracing -- state objects, shader tables, acceleration structures (stub).

use crate::{Dx12Error, Dx12Result};

/// Bottom-level acceleration structure handle. Stub.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DxrBlas(pub u64);

/// Top-level acceleration structure handle. Stub.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DxrTlas(pub u64);

/// Instance description for TLAS builds.
#[derive(Clone, Copy, Debug)]
pub struct DxrInstance {
    pub transform: [f32; 12],
    pub instance_id: u32,
    pub instance_mask: u8,
    pub blas: Option<DxrBlas>,
}

impl Default for DxrInstance {
    fn default() -> Self {
        Self {
            transform: [
                1.0, 0.0, 0.0, 0.0, //
                0.0, 1.0, 0.0, 0.0, //
                0.0, 0.0, 1.0, 0.0,
            ],
            instance_id: 0,
            instance_mask: 0xFF,
            blas: None,
        }
    }
}

/// DXR context. Stub.
#[derive(Debug, Default)]
pub struct DxrContext {
    pub blas_count: u32,
    pub tlas_count: u32,
}

impl DxrContext {
    pub fn new() -> Self {
        Self::default()
    }

    /// Build a BLAS from vertex data.
    pub fn build_blas(&mut self, _vertices: &[u8], _indices: &[u8]) -> Dx12Result<DxrBlas> {
        Err(Dx12Error::NotImplemented("DXR BLAS build"))
    }

    /// Build a TLAS from instances.
    pub fn build_tlas(&mut self, instances: &[DxrInstance]) -> Dx12Result<DxrTlas> {
        if instances.is_empty() {
            return Err(Dx12Error::InvalidParam("TLAS needs at least one instance"));
        }
        Err(Dx12Error::NotImplemented("DXR TLAS build"))
    }

    /// Create the ray tracing pipeline state object.
    pub fn create_raytracing_pso(&mut self, _dxil_lib: &[u8]) -> Dx12Result<u64> {
        Err(Dx12Error::NotImplemented("DXR state object creation"))
    }
}
