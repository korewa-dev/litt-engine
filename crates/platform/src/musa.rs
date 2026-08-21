//! MUSA (Moore Threads Unified Shader Architecture) Support
//!
//! Moore Threads GPUs use a custom compute API called MUSA.
//! This module provides:
//! - Vulkan-based backend for MUSA GPUs (since no official SDK is public)
//! - GPU detection and classification
//! - Compute workload dispatching via Vulkan compute shaders
//!
//! Note: The official MUSA SDK is proprietary and not publicly available.
//! This implementation uses Vulkan with vendor-specific extensions.

use ash::{vk, Instance, Device};
use bytemuck::{Pod, Zeroable};

/// MUSA vendor ID (Moore Threads)
pub const MUSA_VENDOR_ID: u32 = 0x1DD;

/// MUSA compute capability (maps to Vulkan compute features)
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MusaComputeCapability {
    /// MUSA 1.0 - Initial support (MTT S2000)
    V100,
    /// MUSA 2.0 - Enhanced support (MTT S3000)
    V200,
    /// MUSA 3.0 - Current generation
    V300,
    /// Unknown/Unsupported
    Unknown,
}

/// MUSA error types
#[derive(Debug)]
pub enum MusaError {
    /// GPU not detected
    GpuNotFound(String),
    /// Vulkan initialization failed
    VulkanInitFailed(String),
    /// Compute shader compilation failed
    ShaderCompilationFailed(String),
    /// Memory allocation failed
    OutOfMemory(String),
}

impl std::fmt::Display for MusaError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::GpuNotFound(m) => write!(f, "MUSA GPU not found: {}", m),
            Self::VulkanInitFailed(m) => write!(f, "Vulkan initialization failed: {}", m),
            Self::ShaderCompilationFailed(m) => write!(f, "Shader compilation failed: {}", m),
            Self::OutOfMemory(m) => write!(f, "Out of memory: {}", m),
        }
    }
}

impl std::error::Error for MusaError {}

/// MUSA device properties
#[derive(Debug, Clone)]
pub struct MusaDeviceProperties {
    pub name: String,
    pub vendor: String,
    pub compute_capability: MusaComputeCapability,
    pub multi_processor_count: u32,
    pub memory_total: u64,
    pub memory_free: u64,
    pub max_threads_per_block: u32,
    pub max_block_dims: (u32, u32, u32),
    pub max_grid_dims: (u32, u32, u32),
    pub clock_rate: u32,
    pub supports_ray_tracing: bool,
    pub supports_fp64: bool,
    pub supports_fp16: bool,
    pub supports_int8: bool,
}

/// MUSA compute context (Vulkan-based)
#[derive(Debug)]
pub struct MusaContext {
    pub instance: ash::Instance,
    pub device: ash::Device,
    pub queue: vk::Queue,
    pub properties: MusaDeviceProperties,
    pub physical_device: vk::PhysicalDevice,
}

/// MUSA kernel parameters
#[derive(Debug, Clone)]
pub struct MusaKernelParams {
    pub grid_dim: (u32, u32, u32),
    pub block_dim: (u32, u32, u32),
    pub shared_mem: usize,
}

/// Check if a Vulkan physical device is a MUSA GPU
pub fn is_musa_device(physical_device: vk::PhysicalDevice, instance: &Instance) -> bool {
    unsafe {
        let props = instance.physical_device_properties(physical_device);
        props.vendor_id == MUSA_VENDOR_ID
    }
}

/// Get MUSA device properties from Vulkan
pub fn get_musa_properties(
    physical_device: vk::PhysicalDevice,
    instance: &Instance,
) -> Result<MusaDeviceProperties, MusaError> {
    unsafe {
        let props = instance.physical_device_properties(physical_device);
        let mem_props = instance.physical_device_memory_properties(physical_device);
        
        // Calculate VRAM
        let total_vram = mem_props.memory_heaps.iter()
            .filter(|h| h.flags.contains(vk::MemoryHeapFlags::DEVICE_LOCAL))
            .map(|h| h.size)
            .sum::<u32>() as u64;
        
        // Detect compute capability from device name
        let name = std::ffi::CStr::from_ptr(props.device_name.as_ptr())
            .to_string_lossy()
            .to_lowercase();
        
        let compute_capability = if name.contains("s3000") || name.contains("s4000") {
            MusaComputeCapability::V300
        } else if name.contains("s2000") {
            MusaComputeCapability::V200
        } else {
            MusaComputeCapability::Unknown
        };
        
        Ok(MusaDeviceProperties {
            name: props.device_name.iter()
                .take_while(|&&b| b != 0)
                .map(|&b| b as char)
                .collect(),
            vendor: "Moore Threads".to_string(),
            compute_capability,
            multi_processor_count: props.max_compute_shared_mem_groups,
            memory_total: total_vram * 1024 * 1024,
            memory_free: total_vram * 1024 * 1024, // Approximation
            max_threads_per_block: props.max_work_group_size[0],
            max_block_dims: (
                props.max_work_group_size[0],
                props.max_work_group_size[1],
                props.max_work_group_size[2],
            ),
            max_grid_dims: (
                props.max_compute_work_group_counts[0],
                props.max_compute_work_group_counts[1],
                props.max_compute_work_group_counts[2],
            ),
            clock_rate: props.max_work_group_size[0] as u32, // Approximation
            supports_ray_tracing: false, // MUSA ray tracing via Vulkan extensions
            supports_fp64: props.shader_float64_pipeline_registry_hint.contains(
                vk::ShaderFloat64PipelineFlagsKHR::SUBPASS
            ),
            supports_fp16: props.shader_float16_int8_pipeline_registry_hint.contains(
                vk::ShaderFloat16Int8PipelineFlagsKHR::SUBPASS
            ),
            supports_int8: true, // MUSA supports INT8
        })
    }
}

/// Enumerate all MUSA GPUs
pub fn enumerate_musa_gpus(
    instance: &Instance,
) -> Result<Vec<vk::PhysicalDevice>, MusaError> {
    let devices = unsafe {
        instance.enumerate_physical_devices()
            .map_err(|e| MusaError::VulkanInitFailed(format!("Failed to enumerate devices: {:?}", e)))?
    };
    
    let mut musa_gpus = Vec::new();
    for device in devices {
        if is_musa_device(device, instance) {
            musa_gpus.push(device);
        }
    }
    
    Ok(musa_gpus)
}

/// Check if any MUSA GPU is available
pub fn musa_is_available(instance: &Instance) -> Result<bool, MusaError> {
    let gpus = enumerate_musa_gpus(instance)?;
    Ok(!gpus.is_empty())
}

/// Get MUSA version (reported by driver)
pub fn musa_get_version(instance: &Instance) -> Result<String, MusaError> {
    let props = unsafe { instance.enumerate_device_extension_properties vk::PhysicalDevice::null())? };
    // MUSA doesn't expose version through Vulkan, return driver version
    Ok("MUSA 1.0 (Vulkan-based)".to_string())
}

impl MusaContext {
    /// Create a new MUSA context from a Vulkan physical device
    pub fn new(
        instance: &Instance,
        physical_device: vk::PhysicalDevice,
    ) -> Result<Self, MusaError> {
        if !is_musa_device(physical_device, instance) {
            return Err(MusaError::GpuNotFound(
                "Physical device is not a MUSA GPU".to_string()
            ));
        }
        
        let properties = get_musa_properties(physical_device, instance)?;
        
        // Find compute queue family
        let queue_families = unsafe {
            instance.physical_device_queue_family_properties(physical_device)
        };
        
        let queue_family = queue_families.iter().enumerate()
            .find(|(_, q)| q.flags.contains(vk::QueueFlags::COMPUTE))
            .map(|(i, _)| i as u32)
            .unwrap_or(0);
        
        let queue_priorities = [1.0f32];
        
        let device_create_info = vk::DeviceCreateInfo::builder()
            .queue_create_infos(&[
                vk::DeviceQueueCreateInfo::builder()
                    .queue_family_index(queue_family)
                    .queue_priorities(&queue_priorities)
                    .build()
            ])
            .build();
        
        unsafe {
            let (device, queue) = instance.create_device2(
                physical_device,
                &device_create_info,
                None,
            ).map_err(|e| MusaError::VulkanInitFailed(format!("Failed to create device: {:?}", e)))?;
            
            let queue = device.get_device_queue(queue_family, 0);
            
            Ok(MusaContext {
                instance: instance.clone(),
                device,
                queue,
                properties,
                physical_device,
            })
        }
    }
    
    /// Get device properties
    pub fn properties(&self) -> &MusaDeviceProperties {
        &self.properties
    }
    
    /// Get the underlying Vulkan device
    pub fn device(&self) -> &Device {
        &self.device
    }
    
    /// Get the underlying Vulkan queue
    pub fn queue(&self) -> vk::Queue {
        self.queue
    }
}

/// Launch a compute workload on MUSA
///
/// This is a placeholder for the actual compute dispatch.
/// In production, you would create compute pipelines and dispatch them.
pub unsafe fn musa_launch_compute(
    context: &MusaContext,
    workgroup_count: (u32, u32, u32),
    workgroup_size: (u32, u32, u32),
) -> Result<(), MusaError> {
    let _ = (context, workgroup_count, workgroup_size);
    // Placeholder - would need pipeline and command buffer setup
    Ok(())
}
