//! litt-dx12 — DirectX 12 backend for Litt Engine
//!
//! Provides DXGI/DX12 hardware rendering with ray tracing (DXR) support.
//! Targets Windows only; Linux/Android continue using Vulkan.

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

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

/// DX12 backend feature flags
#[derive(Clone, Copy, Debug, Default)]
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
    fn default() -> Self { Self::Hardware }
}

/// Adapter info (GPU identity)
#[derive(Debug)]
pub struct AdapterInfo {
    pub name: String,
    pub vendor_id: u32,
    pub device_id: u32,
    pub description: String,
    pub driver_version: u64,
    pub feature_level: FeatureLevel,
    pub ray_tracing_support: bool,
}

/// D3D feature level
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FeatureLevel {
    D12_0,
    D12_1,
    D12_2,
    D11_0,
    D11_1,
}

impl std::fmt::Display for FeatureLevel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::D12_0 => write!(f, "Direct3D 12.0"),
            Self::D12_1 => write!(f, "Direct3D 12.1"),
            Self::D12_2 => write!(f, "Direct3D 12.2"),
            Self::D11_0 => write!(f, "Direct3D 11.0"),
            Self::D11_1 => write!(f, "Direct3D 11.1"),
        }
    }
}

/// GPU vendor
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum GpuVendor {
    #[default]
    Unknown,
    Amd,
    Nvidia,
    Intel,
    MooreThreads,
    Samsung,
    Qualcomm,
    Other(u32),
}

impl GpuVendor {
    pub fn from_vendor_id(vendor_id: u32) -> Self {
        match vendor_id {
            0x1002 => Self::Amd,
            0x10DE => Self::Nvidia,
            0x8086 => Self::Intel,
            0x01DD => Self::MooreThreads,
            0x1AE => Self::Samsung,
            0x5143 => Self::Qualcomm,
            _ => Self::Other(vendor_id),
        }
    }
}

/// Error types for DX12 operations
#[derive(Debug)]
pub enum Dx12Error {
    DxgiFactoryCreation(String),
    AdapterEnumeration(String),
    DeviceCreation(String),
    FeatureLevelUnsupported(String),
    SwapchainCreation(String),
    CommandQueueCreation(String),
    DescriptorHeapCreation(String),
    PipelineCreation(String),
    ShaderCompilation(String),
    RayTracingSetup(String),
    ResourceAllocation(String),
    WarpUnavailable,
    InvalidParameter(String),
    DxError(String),
}

impl std::fmt::Display for Dx12Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::DxgiFactoryCreation(m) => write!(f, "DXGI factory creation failed: {}", m),
            Self::AdapterEnumeration(m) => write!(f, "Adapter enumeration failed: {}", m),
            Self::DeviceCreation(m) => write!(f, "Device creation failed: {}", m),
            Self::FeatureLevelUnsupported(m) => write!(f, "Feature level unsupported: {}", m),
            Self::SwapchainCreation(m) => write!(f, "Swapchain creation failed: {}", m),
            Self::CommandQueueCreation(m) => write!(f, "Command queue creation failed: {}", m),
            Self::DescriptorHeapCreation(m) => write!(f, "Descriptor heap creation failed: {}", m),
            Self::PipelineCreation(m) => write!(f, "Pipeline creation failed: {}", m),
            Self::ShaderCompilation(m) => write!(f, "Shader compilation failed: {}", m),
            Self::RayTracingSetup(m) => write!(f, "Ray tracing setup failed: {}", m),
            Self::ResourceAllocation(m) => write!(f, "Resource allocation failed: {}", m),
            Self::WarpUnavailable => write!(f, "WARP software rasterizer unavailable"),
            Self::InvalidParameter(m) => write!(f, "Invalid parameter: {}", m),
            Self::DxError(m) => write!(f, "DX12 error: {}", m),
        }
    }
}

impl std::error::Error for Dx12Error {}
impl From<String> for Dx12Error { fn from(s: String) -> Self { Self::DxError(s) } }
impl From<&str> for Dx12Error { fn from(s: &str) -> Self { Self::DxError(s.to_string()) } }
