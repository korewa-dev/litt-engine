// Litt Engine Library
// Re-export key types for the GUI

pub use litt_math::*;
pub use litt_platform::*;
pub use litt_vulkan::*;
pub use litt_renderer::*;
pub use litt_pathtracer::*;
pub use litt_fidelityfx::*;
pub use litt_dx12::*;

pub use config::*;

// Graphics backend abstraction
pub mod graphics;
pub use graphics::{GraphicsBackend, select_backend, get_gpu_info};

pub mod config {
    // Re-export config types
    pub use crate::math::{Vec3, Vec4, Mat4, Ray, BoundingBox, Triangle};
    pub use crate::pathtracer::{Scene, Camera, Material, HitInfo};
    pub use crate::renderer::{Renderer, Swapchain};
    pub use crate::vulkan::{VulkanDevice, Instance, PhysicalDevice};
    
    /// Graphics backend selection
    pub use crate::graphics::{GraphicsFeatures, Dx12Features, FeatureLevel};
}
