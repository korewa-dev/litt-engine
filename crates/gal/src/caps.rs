//! Per-backend capability report.
//!
//! Game code queries this instead of `#ifdef`-ing on API names: "can I do
//! RT shadows here?" works identically whether the active backend is
//! Vulkan 1.3, DXR, or AGS-tuned Vulkan.

use crate::backend::BackendKind;

/// Feature matrix reported by every device.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Capabilities {
    /// Hardware ray tracing (RT pipelines, BLAS/TLAS).
    pub ray_tracing: bool,
    /// Compute shaders.
    pub compute: bool,
    /// Mesh shaders.
    pub mesh_shaders: bool,
    /// Shader float16 (via core version or extension).
    pub shader_fp16: bool,
    /// Shader int8.
    pub shader_int8: bool,
    /// Wave/subgroup intrinsics available.
    pub wave_intrinsics: bool,
    /// Bindless descriptor arrays of unbounded size.
    pub bindless: bool,
    /// Async compute queues distinct from graphics queue.
    pub async_compute: bool,
}

impl Capabilities {
    /// Conservative capability set for the headless null backend:
    /// everything is "supported" logically so tests exercise all paths.
    pub const NULL: Capabilities = Capabilities {
        ray_tracing: true,
        compute: true,
        mesh_shaders: true,
        shader_fp16: true,
        shader_int8: true,
        wave_intrinsics: true,
        bindless: true,
        async_compute: true,
    };

    /// Expected baseline for a Vulkan 1.3 device (RT depends on GPU).
    pub const VULKAN_BASELINE: Capabilities = Capabilities {
        ray_tracing: false, // queried at runtime
        compute: true,
        mesh_shaders: false,
        shader_fp16: true,
        shader_int8: true,
        wave_intrinsics: true,
        bindless: true,
        async_compute: true,
    };

    /// DX12 Ultimate baseline (DXR present on FL 12.0+ hardware).
    pub const DX12_BASELINE: Capabilities = Capabilities {
        ray_tracing: true,
        compute: true,
        mesh_shaders: true,
        shader_fp16: true,
        shader_int8: true,
        wave_intrinsics: true,
        bindless: true,
        async_compute: true,
    };

    /// AGS rides Vulkan and adds AMD-specific paths on top.
    pub const AGS_BASELINE: Capabilities = Capabilities {
        ray_tracing: false,
        compute: true,
        mesh_shaders: false,
        shader_fp16: true,
        shader_int8: true,
        wave_intrinsics: true,
        bindless: true,
        async_compute: true,
    };

    /// Baseline capabilities advertised per backend kind.
    pub const fn baseline(kind: BackendKind) -> Capabilities {
        match kind {
            BackendKind::Null => Capabilities::NULL,
            BackendKind::Vulkan => Capabilities::VULKAN_BASELINE,
            BackendKind::Dx12 => Capabilities::DX12_BASELINE,
            BackendKind::Ags => Capabilities::AGS_BASELINE,
        }
    }

    /// Intersection of two sets -- used when mirroring work across devices
    /// to know which commands can run everywhere.
    pub const fn intersect(self, other: Capabilities) -> Capabilities {
        Capabilities {
            ray_tracing: self.ray_tracing && other.ray_tracing,
            compute: self.compute && other.compute,
            mesh_shaders: self.mesh_shaders && other.mesh_shaders,
            shader_fp16: self.shader_fp16 && other.shader_fp16,
            shader_int8: self.shader_int8 && other.shader_int8,
            wave_intrinsics: self.wave_intrinsics && other.wave_intrinsics,
            bindless: self.bindless && other.bindless,
            async_compute: self.async_compute && other.async_compute,
        }
    }
}
