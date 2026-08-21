//! Graphics backend abstraction — Vulkan or DX12
//!
//! Selects the appropriate graphics backend at runtime based on platform
//! and availability. Vulkan is primary; DX12 is the Windows-native path.

/// Graphics backend feature flags
#[derive(Clone, Debug, Default)]
pub struct GraphicsFeatures {
    pub ray_tracing: bool,
    pub mesh_shader: bool,
    pub variable_rate_shading: bool,
    pub acceleration_structure: bool,
}

/// DX12 feature levels
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FeatureLevel {
    DX12_1,
    DX12_2,
    DX12_3,
    DX12_4,
    DX12_5,
    DX12_6,
    DX12_7,
    DX12_8,
    DX12_9,
    DX12_10,
    DX12_11,
    DX12_12,
}

/// DX12 specific features
#[derive(Clone, Debug, Default)]
pub struct Dx12Features {
    pub mesh_shader: bool,
    pub raytracing: bool,
    pub variable_rate_shading: bool,
    pub sampler_feedback: bool,
}

/// Graphics backend trait
pub trait GraphicsBackend: Send + Sync {
    /// Get the backend name
    fn name(&self) -> &str;
    
    /// Check if ray tracing is supported
    fn supports_ray_tracing(&self) -> bool;
    
    /// Check if mesh shaders are supported
    fn supports_mesh_shaders(&self) -> bool;
    
    /// Get the adapter info
    fn adapter_info(&self) -> &str;
    
    /// Initialize the backend (after window creation)
    fn initialize(&mut self, width: u32, height: u32) -> Result<(), String>;
    
    /// Begin a new frame
    fn begin_frame(&mut self) -> Result<(), String>;
    
    /// Record render commands
    fn render(&mut self, scene: &crate::pathtracer::Scene, camera: &crate::pathtracer::Camera) -> Result<(), String>;
    
    /// Present the frame
    fn present(&mut self) -> Result<(), String>;
    
    /// End the frame
    fn end_frame(&mut self) -> Result<(), String>;
    
    /// Shutdown the backend
    fn shutdown(&mut self) -> Result<(), String>;
}

/// Vulkan backend wrapper
#[cfg(feature = "vulkan")]
pub mod vulkan {
    use super::*;
    use crate::vulkan::*;
    
    pub struct VulkanBackend {
        pub instance: Option<Instance>,
        pub device: Option<VulkanDevice>,
        pub swapchain: Option<Swapchain>,
        pub command_pool: Option<CommandPool>,
        pub render_pass: Option<RenderPass>,
        pub descriptor_pool: Option<DescriptorPool>,
        pub features: GraphicsFeatures,
    }
    
    impl VulkanBackend {
        pub fn new() -> Self {
            Self {
                instance: None,
                device: None,
                swapchain: None,
                command_pool: None,
                render_pass: None,
                descriptor_pool: None,
                features: GraphicsFeatures::default(),
            }
        }
    }
    
    impl GraphicsBackend for VulkanBackend {
        fn name(&self) -> &str { "Vulkan" }
        
        fn supports_ray_tracing(&self) -> bool {
            self.features.ray_tracing
        }
        
        fn supports_mesh_shaders(&self) -> bool {
            false
        }
        
        fn adapter_info(&self) -> &str {
            "AMD Radeon / Intel Arc / Moore Threads"
        }
        
        fn initialize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
            Ok(())
        }
        
        fn begin_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn render(&mut self, _scene: &crate::pathtracer::Scene, _camera: &crate::pathtracer::Camera) -> Result<(), String> { Ok(()) }
        fn present(&mut self) -> Result<(), String> { Ok(()) }
        fn end_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn shutdown(&mut self) -> Result<(), String> { Ok(()) }
    }
}

/// DX12 backend wrapper
#[cfg(feature = "dx12")]
pub mod dx12 {
    use super::*;
    use crate::dx12::*;
    
    pub struct Dx12Backend {
        pub device: Option<Device>,
        pub features: GraphicsFeatures,
    }
    
    impl Dx12Backend {
        pub fn new() -> Self {
            Self {
                device: None,
                features: GraphicsFeatures::default(),
            }
        }
        
        pub fn init(&mut self) -> Result<(), String> {
            // DX12 initialization
            Ok(())
        }
    }
    
    impl GraphicsBackend for Dx12Backend {
        fn name(&self) -> &str { "DX12" }
        
        fn supports_ray_tracing(&self) -> bool {
            self.features.ray_tracing
        }
        
        fn supports_mesh_shaders(&self) -> bool {
            true
        }
        
        fn adapter_info(&self) -> &str {
            "DX12 (Windows native)"
        }
        
        fn initialize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
            self.init()
        }
        
        fn begin_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn render(&mut self, _scene: &crate::pathtracer::Scene, _camera: &crate::pathtracer::Camera) -> Result<(), String> { Ok(()) }
        fn present(&mut self) -> Result<(), String> { Ok(()) }
        fn end_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn shutdown(&mut self) -> Result<(), String> { Ok(()) }
    }
}

/// Select the best graphics backend
pub fn select_backend() -> Result<Box<dyn GraphicsBackend>, String> {
    #[cfg(feature = "dx12")]
    {
        let mut backend = dx12::Dx12Backend::new();
        if backend.init().is_ok() {
            return Ok(Box::new(backend));
        }
    }
    
    #[cfg(feature = "vulkan")]
    {
        let mut backend = vulkan::VulkanBackend::new();
        backend.initialize(1280, 720).map_err(|e| e.to_string())?;
        return Ok(Box::new(backend));
    }
    
    #[allow(unreachable_code)]
    Err("No graphics backend available".to_string())
}

/// Get the detected GPU info
pub fn get_gpu_info() -> String {
    #[cfg(feature = "dx12")]
    {
        "DX12 (Windows native)".to_string()
    }
    #[cfg(all(not(feature = "dx12"), feature = "vulkan"))]
    {
        "Vulkan".to_string()
    }
    #[allow(unreachable_code)]
    "Unknown".to_string()
}
