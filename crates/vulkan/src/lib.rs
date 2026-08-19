//! Minimal Vulkan backend using ash.
//! No abstraction layer — explicit Vulkan calls only.

pub mod instance;
pub mod device;
pub mod swapchain;
pub mod allocator;
pub mod pipeline;
pub mod ray_tracing;

pub use instance::*;
pub use device::*;
pub use swapchain::*;
pub use allocator::*;
pub use pipeline::*;
pub use ray_tracing::*;

use ash::{vk, extensions::khr};
use bytemuck::{Pod, Zeroable};

// =============================================================================
// Memory type helpers
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
/// GPU vendor type
#[derive(Clone, Copy, Debug, Default)]
pub enum GpuVendor {
    #[default]
    Unknown,
    Amd,
    MooreThreads,
    Intel,
    Samsung,
    Nvidia,
    Other(u32),
}

pub struct BufferCreateInfo {
    pub size: u64,
    pub usage: vk::BufferUsageFlags,
    pub memory_type: u32,
    pub host_accessible: bool,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct ImageCreateInfo {
    pub extent: [u32; 3],
    pub format: vk::Format,
    pub usage: vk::ImageUsageFlags,
    pub memory_type: u32,
    pub mip_levels: u32,
    pub array_layers: u32,
}

// =============================================================================
// Buffer
// =============================================================================

#[derive(Debug)]
pub struct Buffer {
    pub handle: vk::Buffer,
    pub memory: vk::DeviceMemory,
    pub size: u64,
    pub allocation: Option<AllocatorHandle>,
}

impl Buffer {
    pub fn empty(vk: &VulkanDevice) -> Self {
        Self {
            handle: vk::Buffer::null(),
            memory: vk::DeviceMemory::null(),
            size: 0,
            allocation: None,
        }
    }
}

// =============================================================================
// Image
// =============================================================================

#[derive(Debug)]
pub struct Image {
    pub handle: vk::Image,
    pub memory: vk::DeviceMemory,
    pub view: vk::ImageView,
    pub format: vk::Format,
    pub extent: [u32; 3],
    pub allocation: Option<AllocatorHandle>,
}

impl Image {
    pub fn empty(vk: &VulkanDevice) -> Self {
        Self {
            handle: vk::Image::null(),
            memory: vk::DeviceMemory::null(),
            view: vk::ImageView::null(),
            format: vk::Format::UNKNOWN,
            extent: [0; 3],
            allocation: None,
        }
    }
}

// =============================================================================
// Descriptor Set Layout & Descriptor Pool
// =============================================================================

#[derive(Debug)]
pub struct DescriptorSetLayout {
    pub layout: vk::DescriptorSetLayout,
}

#[derive(Debug)]
pub struct DescriptorPool {
    pub pool: vk::DescriptorPool,
    pub max_sets: u32,
}

// =============================================================================
// Pipeline
// =============================================================================

#[derive(Debug)]
pub struct GraphicsPipeline {
    pub pipeline: vk::Pipeline,
    pub layout: vk::PipelineLayout,
}

#[derive(Debug)]
pub struct ComputePipeline {
    pub pipeline: vk::Pipeline,
    pub layout: vk::PipelineLayout,
}

#[derive(Debug)]
pub struct RayTracingPipeline {
    pub pipeline: vk::Pipeline,
    pub pipeline_layout: vk::PipelineLayout,
    pub shader_group_handle_size: u32,
}

// =============================================================================
// Swapchain
// =============================================================================

#[derive(Debug)]
pub struct Swapchain {
    pub swapchain: vk::SwapchainKHR,
    pub images: Vec<vk::Image>,
    pub views: Vec<vk::ImageView>,
    pub extents: [u32; 3],
    pub format: vk::Format,
    pub image_count: u32,
}

// =============================================================================
// Synchronization
// =============================================================================

#[derive(Debug)]
pub struct Fence {
    pub fence: vk::Fence,
}

#[derive(Debug)]
pub struct Semaphore {
    pub semaphore: vk::Semaphore,
}

// =============================================================================
// Acceleration Structure
// =============================================================================

#[derive(Debug)]
pub struct AccelerationStructure {
    pub handle: vk::AccelerationStructureKHR,
    pub memory: vk::DeviceMemory,
    pub size: u64,
    pub allocation: Option<AllocatorHandle>,
}

// =============================================================================
// Allocator handle (internal)
// =============================================================================

#[derive(Clone, Copy, Debug)]
pub struct AllocatorHandle {
    pub device_ptr: u64,
}

// =============================================================================
// Device info
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct QueueFamilies {
    pub graphics: u32,
    pub compute: u32,
    pub transfer: u32,
    pub rt: u32,
}

#[derive(Debug)]
pub struct SurfaceCapabilities {
    pub min_image_count: u32,
    pub max_image_count: u32,
    pub current_extent: [u32; 2],
    pub supported_formats: Vec<vk::Format>,
    pub supported_usage_flags: vk::ImageUsageFlags,
}
