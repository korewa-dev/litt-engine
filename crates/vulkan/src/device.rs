//! Logical device creation with AMD-specific features enabled.
//! Migrated to ash 0.38 (struct literals, no builders).

use crate::allocator::GpuAllocator;
use crate::ags::{AmgInfo, GpuProperties};

/// Logical Vulkan device with all necessary handles
pub struct VulkanDevice {
    pub instance: ash::Instance,
    pub entry: std::sync::Arc<ash::Entry>,
    pub device: ash::Device,
    pub physical_device: vk::PhysicalDevice,
    pub surface: vk::SurfaceKHR,
    pub surface_loader: ash::khr::surface::Instance,
    pub swapchain_loader: ash::khr::swapchain::Device,
    pub draw_queue: vk::Queue,
    pub compute_queue: vk::Queue,
    pub transfer_queue: vk::Queue,
    pub graphics_family: u32,
    pub compute_family: u32,
    pub transfer_family: u32,
    pub memory_properties: vk::PhysicalDeviceMemoryProperties,
    pub properties: vk::PhysicalDeviceProperties,
    pub ext_features: PhysicalDeviceExtensions,
    /// Memory allocator
    pub allocator: GpuAllocator,
    /// AMD AGS information
    pub ags_info: AmgInfo,
}

use ash::vk;

/// Extension features required for this engine
#[derive(Default, Clone, Copy, Debug)]
pub struct PhysicalDeviceExtensions {
    pub ray_tracing: bool,
    pub acceleration_structure: bool,
    pub descriptor_indexing: bool,
    pub push_descriptor: bool,
    pub sustained_encoding: bool,
    pub shader_float16: bool,
    pub shader_int8: bool,
}

impl VulkanDevice {
    /// Create a new logical device from a physical device
    pub unsafe fn new(
        entry: &std::sync::Arc<ash::Entry>,
        instance: &ash::Instance,
        physical: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
        queue_families: &crate::instance::QueueFamilies,
    ) -> Result<Self, String> {
        let surface_loader = ash::khr::surface::Instance::new(entry, instance);

        // Get device properties (0.38: unsafe value-returning queries)
        let properties = instance.get_physical_device_properties(physical);
        let mem_properties = instance.get_physical_device_memory_properties(physical);

        // Query extensions
        let extensions = instance
            .enumerate_device_extension_properties(physical)
            .unwrap_or_default();
        let ext_names: Vec<String> = extensions
            .iter()
            .map(|e| {
                std::ffi::CStr::from_ptr(e.extension_name.as_ptr())
                    .to_string_lossy()
                    .into_owned()
            })
            .collect();

        let has_rt = ext_names.iter().any(|n| n == "VK_KHR_ray_tracing_pipeline");
        let has_as = ext_names.iter().any(|n| n == "VK_KHR_acceleration_structure");
        let has_di = ext_names.iter().any(|n| n == "VK_EXT_descriptor_indexing");
        let has_pd = ext_names.iter().any(|n| n == "VK_EXT_push_descriptor");

        let ext_features = PhysicalDeviceExtensions {
            ray_tracing: has_rt,
            acceleration_structure: has_as,
            descriptor_indexing: has_di,
            push_descriptor: has_pd,
            sustained_encoding: false,
            shader_float16: true,
            shader_int8: true,
        };

        // Queue creation -- one queue per distinct family
        let queue_priorities = [1.0f32];
        let mut family_set = std::collections::HashSet::new();
        let mut families: Vec<u32> = Vec::new();
        for f in [queue_families.graphics, queue_families.compute, queue_families.transfer] {
            if family_set.insert(f) {
                families.push(f);
            }
        }
        let queue_create_infos: Vec<vk::DeviceQueueCreateInfo> = families
            .iter()
            .map(|f| vk::DeviceQueueCreateInfo {
                queue_family_index: *f,
                queue_count: queue_priorities.len() as u32,
                p_queue_priorities: queue_priorities.as_ptr(),
                ..Default::default()
            })
            .collect();

        // Device extensions
        let mut device_exts: Vec<std::ffi::CString> = vec![
            std::ffi::CString::new("VK_KHR_swapchain").unwrap(),
        ];

        if has_rt {
            for name in [
                "VK_KHR_ray_tracing_pipeline",
                "VK_KHR_acceleration_structure",
                "VK_KHR_deferred_host_operations",
            ] {
                device_exts.push(std::ffi::CString::new(name).unwrap());
            }
        }
        if has_di {
            device_exts.push(std::ffi::CString::new("VK_EXT_descriptor_indexing").unwrap());
        }
        // AMD-specific extensions
        if ext_names.iter().any(|n| n == "VK_AMD_shader_core_properties") {
            device_exts.push(std::ffi::CString::new("VK_AMD_shader_core_properties").unwrap());
        }
        if ext_names.iter().any(|n| n == "VK_EXT_pipeline_creation_cache_control") {
            device_exts.push(
                std::ffi::CString::new("VK_EXT_pipeline_creation_cache_control").unwrap(),
            );
        }

        let ext_ptrs: Vec<*const i8> =
            device_exts.iter().map(|s| s.as_ptr()).collect();

        // Core 1.2 features
        let mut features12 = vk::PhysicalDeviceVulkan12Features {
            descriptor_indexing: ash::vk::TRUE,
            timeline_semaphore: ash::vk::TRUE,
            ..Default::default()
        };

        // Ray tracing feature chain: 12 -> RT -> AS
        let mut rt_features = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR {
            ray_tracing_pipeline: ash::vk::TRUE,
            ..Default::default()
        };
        let mut as_features = vk::PhysicalDeviceAccelerationStructureFeaturesKHR {
            acceleration_structure: ash::vk::TRUE,
            ..Default::default()
        };
        if has_rt && has_as {
            rt_features.p_next =
                &mut as_features as *mut _ as *mut std::ffi::c_void;
            features12.p_next =
                &mut rt_features as *mut _ as *mut std::ffi::c_void;
        }

        let info = vk::DeviceCreateInfo {
            queue_create_info_count: queue_create_infos.len() as u32,
            p_queue_create_infos: queue_create_infos.as_ptr(),
            enabled_extension_count: ext_ptrs.len() as u32,
            pp_enabled_extension_names: ext_ptrs.as_ptr(),
            p_next: &mut features12 as *mut _ as *mut std::ffi::c_void,
            ..Default::default()
        };

        let device = instance
            .create_device(physical, &info, None)
            .map_err(|e| format!("Failed to create device: {e:?}"))?;

        let swapchain_loader = ash::khr::swapchain::Device::new(instance, &device);

        let draw_queue = device.get_device_queue(queue_families.graphics, 0);
        let compute_queue = if queue_families.compute != queue_families.graphics {
            device.get_device_queue(queue_families.compute, 0)
        } else {
            draw_queue
        };
        let transfer_queue = if queue_families.transfer != queue_families.graphics {
            device.get_device_queue(queue_families.transfer, 0)
        } else {
            draw_queue
        };

        // Initialize memory allocator
        let allocator = GpuAllocator::new(&device, physical, instance)?;

        // Initialize AMD AGS info
        let ags_props = GpuProperties::from_vulkan(instance, physical);
        let ags_info = AmgInfo::from_gpu(&ags_props);

        Ok(Self {
            instance: instance.clone(),
            entry: entry.clone(),
            device,
            physical_device: physical,
            surface,
            surface_loader,
            swapchain_loader,
            draw_queue,
            compute_queue,
            transfer_queue,
            graphics_family: queue_families.graphics,
            compute_family: queue_families.compute,
            transfer_family: queue_families.transfer,
            memory_properties: mem_properties,
            properties,
            ext_features,
            allocator,
            ags_info,
        })
    }

    /// Get memory type index for given properties
    pub fn memory_type_index(
        &self,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Option<u32> {
        (0..self.memory_properties.memory_type_count).find(|&i| (type_filter & (1 << i)) != 0
                && self.memory_properties.memory_types[i as usize]
                    .property_flags
                    .contains(properties))
    }
}

impl Drop for VulkanDevice {
    fn drop(&mut self) {
        unsafe {
            self.device.device_wait_idle().ok();
            self.device.destroy_device(None);
        }
    }
}
