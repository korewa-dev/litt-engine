//! Graphics backend abstraction — Vulkan or DX12
//!
//! Selects the appropriate graphics backend at runtime based on platform
//! and availability. Vulkan is primary; DX12 is the Windows-native path.

use std::sync::Arc;

/// Graphics backend feature flags
#[derive(Clone, Debug, Default)]
pub struct GraphicsFeatures {
    pub ray_tracing: bool,
    pub mesh_shader: bool,
    pub variable_rate_shading: bool,
    pub acceleration_structure: bool,
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
            false // Not supported in current Vulkan backend
        }
        
        fn adapter_info(&self) -> &str {
            "AMD Radeon / Intel Arc / Moore Threads"
        }
        
        fn initialize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
            // Vulkan init is handled by existing code
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
        pub factory: Option<*mut winapi::um::dxgi::IDXGIFactory2>,
        pub adapters: Vec<AdapterInfo>,
        pub device: Option<Dx12Device>,
        pub swapchain: Option<Swapchain>,
        pub command_context: Option<CommandContext>,
        pub features: GraphicsFeatures,
    }
    
    impl Dx12Backend {
        pub fn new() -> Self {
            Self {
                factory: None,
                adapters: Vec::new(),
                device: None,
                swapchain: None,
                command_context: None,
                features: GraphicsFeatures::default(),
            }
        }
        
        /// Initialize DX12 backend — enumerate adapters and create device
        pub fn init(&mut self) -> Result<(), String> {
            unsafe {
                // Create DXGI factory
                let factory = create_dxgi_factory().map_err(|e| e.to_string())?;
                self.factory = Some(factory);
                
                // Enumerate adapters
                let adapters = enumerate_adapters(factory).map_err(|e| e.to_string())?;
                self.adapters = adapters;
                
                // Select best adapter
                let idx = select_best_adapter(&self.adapters).map_err(|e| e.to_string())?;
                let selected = get_adapter_info(&self.adapters, idx).map_err(|e| e.to_string())?;
                
                // Create device
                let adapter = self.adapters[idx as usize]
                    .name.clone(); // Note: in real impl, pass the actual adapter pointer
                let device = create_device(std::ptr::null_mut(), true)
                    .map_err(|e| e.to_string())?;
                self.device = Some(device);
                
                // Check features
                if let Some(ref dev) = self.device {
                    self.features.ray_tracing = check_ray_tracing_support(dev);
                    self.features.mesh_shader = false; // Would need D3D12_FEATURE_D3D12_OPTIONS12
                }
            }
            Ok(())
        }
    }
    
    impl GraphicsBackend for Dx12Backend {
        fn name(&self) -> &str { "DX12" }
        
        fn supports_ray_tracing(&self) -> bool {
            self.features.ray_tracing
        }
        
        fn supports_mesh_shaders(&self) -> bool {
            self.features.mesh_shader
        }
        
        fn adapter_info(&self) -> &str {
            self.adapters.first().map(|a| &a.name).unwrap_or("Unknown")
        }
        
        fn initialize(&mut self, width: u32, height: u32) -> Result<(), String> {
            unsafe {
                if let (Some(factory), Some(ref dev)) = (self.factory, self.device) {
                    // Create swapchain
                    let swapchain = Swapchain::create(
                        *factory,
                        dev.device,
                        std::ptr::null_mut(), // hwnd would come from platform
                        width,
                        height,
                        2, // Double buffering
                    ).map_err(|e| e.to_string())?;
                    self.swapchain = Some(swapchain);
                    
                    // Create command context
                    let cmd_ctx = CommandContext::new(
                        dev.device,
                        dev.graphics_queue,
                        2,
                    ).map_err(|e| e.to_string())?;
                    self.command_context = Some(cmd_ctx);
                }
            }
            Ok(())
        }
        
        fn begin_frame(&mut self) -> Result<(), String> {
            if let Some(ref mut ctx) = self.command_context {
                ctx.reset_allocator().map_err(|e| e.to_string())?;
            }
            Ok(())
        }
        
        fn render(&mut self, _scene: &crate::pathtracer::Scene, _camera: &crate::pathtracer::Camera) -> Result<(), String> {
            // DX12 render recording would go here
            Ok(())
        }
        
        fn present(&mut self) -> Result<(), String> {
            if let Some(ref mut sc) = self.swapchain {
                sc.present(1).map_err(|e| e.to_string())?;
            }
            Ok(())
        }
        
        fn end_frame(&mut self) -> Result<(), String> {
            if let Some(ref mut ctx) = self.command_context {
                ctx.signal_and_wait().map_err(|e| e.to_string())?;
                ctx.next_frame();
            }
            Ok(())
        }
        
        fn shutdown(&mut self) -> Result<(), String> {
            // DX12 cleanup
            Ok(())
        }
    }
}

/// Select the best available graphics backend
pub fn select_backend() -> Result<Box<dyn GraphicsBackend>, String> {
    #[cfg(feature = "dx12")]
    {
        // Try DX12 first on Windows
        let mut backend = dx12::Dx12Backend::new();
        if backend.init().is_ok() {
            return Ok(Box::new(backend));
        }
    }
    
    #[cfg(feature = "vulkan")]
    {
        // Fallback to Vulkan
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
        // DX12 GPU info would be queried here
        "DX12 (Windows native)".to_string()
    }
    #[cfg(all(not(feature = "dx12"), feature = "vulkan"))]
    {
        "Vulkan".to_string()
    }
    #[allow(unreachable_code)]
    "Unknown".to_string()
}
