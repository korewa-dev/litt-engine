//! Logical device creation with AMD-specific features enabled.

use ash::{vk, Instance, Device};
use super::*;

/// Logical Vulkan device with all necessary handles
pub struct VulkanDevice {
    pub instance: ash::Instance,
    pub device: ash::Device,
    pub physical_device: vk::PhysicalDevice,
    pub surface: vk::SurfaceKHR,
    pub surface_loader: khr::Surface,
    pub swapchain_loader: khr::Swapchain,
    pub draw_queue: vk::Queue,
    pub compute_queue: vk::Queue,
    pub transfer_queue: vk::Queue,
    pub graphics_family: u32,
    pub compute_family: u32,
    pub transfer_family: u32,
    pub memory_properties: vk::PhysicalDeviceMemoryProperties,
    pub properties: vk::PhysicalDeviceProperties,
    pub features: vk::PhysicalDeviceFeatures,
    pub ext_features: PhysicalDeviceExtensions,
}

/// Extension features required for this engine
#[derive(Default)]
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
        instance: &Instance,
        physical: vk::PhysicalDevice,
        surface: vk::SurfaceKHR,
        queue_families: &QueueFamilies,
    ) -> Result<Self, String> {
        let surface_loader = khr::Surface::new(instance.clone());
        let swapchain_loader = khr::Swapchain::new(instance.clone(), &surface_loader);

        // Get device properties
        let properties = instance.physical_device_properties(physical);
        let mem_properties = instance.physical_device_memory_properties(physical);

        // Query extensions
        let extensions = instance.enumerate_device_extension_properties(physical).unwrap_or_default();
        let ext_names: Vec<String> = extensions.iter().map(|e| {
            std::ffi::CStr::from_ptr(e.extension_name.as_ptr())
                .to_string_lossy().into_owned()
        }).collect();

        // Check required extensions
        let has_rt = ext_names.contains(&"VK_KHR_ray_tracing_pipeline".to_string());
        let has_as = ext_names.contains(&"VK_KHR_acceleration_structure".to_string());
        let has_di = ext_names.contains(&"VK_EXT_descriptor_indexing".to_string());
        let has_pd = ext_names.contains(&"VK_EXT_push_descriptor".to_string());
        let has_se = ext_names.contains(&"VK_KHR_sustained_fast_encoding".to_string());
        let has_f16 = ext_names.contains(&"VK_KHR_shader_float16_int8".to_string());
        let has_i8 = ext_names.contains(&"VK_KHR_shader_float_controls".to_string());

        let mut ext_features = PhysicalDeviceExtensions {
            ray_tracing: has_rt,
            acceleration_structure: has_as,
            descriptor_indexing: has_di,
            push_descriptor: has_pd,
            sustained_encoding: has_se,
            shader_float16: has_f16,
            shader_int8: has_i8,
        };

        // Queue creation
        let queue_priorities = [1.0f32];
        let mut queue_create_infos = Vec::new();
        let mut used_families = std::collections::HashSet::new();

        if !used_families.contains(&queue_families.graphics) {
            queue_create_infos.push(
                vk::DeviceQueueCreateInfo::builder()
                    .queue_family_index(queue_families.graphics)
                    .queue_priorities(&queue_priorities)
                    .build()
            );
            used_families.insert(queue_families.graphics);
        }

        // Add compute queue if different
        if queue_families.compute != queue_families.graphics && !used_families.contains(&queue_families.compute) {
            queue_create_infos.push(
                vk::DeviceQueueCreateInfo::builder()
                    .queue_family_index(queue_families.compute)
                    .queue_priorities(&queue_priorities)
                    .build()
            );
            used_families.insert(queue_families.compute);
        }

        // Add transfer queue if different
        if queue_families.transfer != queue_families.graphics && !used_families.contains(&queue_families.transfer) {
            queue_create_infos.push(
                vk::DeviceQueueCreateInfo::builder()
                    .queue_family_index(queue_families.transfer)
                    .queue_priorities(&queue_priorities)
                    .build()
            );
        }

        // Instance extensions to enable
        let instance_extensions: Vec<std::ffi::CString> = vec![
            std::ffi::CString::new("VK_KHR_swapchain").unwrap(),
            std::ffi::CString::new("VK_KHR_get_physical_device_properties2").unwrap(),
        ];

        // Device extensions
        let mut device_exts: Vec<std::ffi::CString> = vec![
            std::ffi::CString::new("VK_KHR_swapchain").unwrap(),
            std::ffi::CString::new("VK_KHR_descriptor_update_template").unwrap(),
        ];

        if has_rt {
            device_exts.push(std::ffi::CString::new("VK_KHR_ray_tracing_pipeline").unwrap());
            device_exts.push(std::ffi::CString::new("VK_KHR_acceleration_structure").unwrap());
            device_exts.push(std::ffi::CString::new("VK_KHR_deferred_host_operations").unwrap());
        }

        if has_di {
            device_exts.push(std::ffi::CString::new("VK_EXT_descriptor_indexing").unwrap());
        }

        // AMD-specific extensions
        if ext_names.contains(&"VK_AMD_shader_core_properties".to_string()) {
            device_exts.push(std::ffi::CString::new("VK_AMD_shader_core_properties").unwrap());
        }
        if ext_names.contains(&"VK_EXT_pipeline_creation_cache_control".to_string()) {
            device_exts.push(std::ffi::CString::new("VK_EXT_pipeline_creation_cache_control").unwrap());
        }

        // Features for Vulkan 1.2+
        let mut features2 = vk::PhysicalDeviceVulkan12Features::builder()
            .build();

        // Features for descriptor indexing
        if has_di {
            features2.descriptor_indexing = true;
            features2.shader_uniform_texel_buffer_array = true;
            features2.shader_sampled_image_array = true;
            features2.shader_storage_buffer_array = true;
            features2.shader_storage_image_array = true;
            features2.shader_input_attachment_array = true;
            features2.shader_atomic_float_add = true;
            features2.push_descriptor_configuration = has_pd;
        }

        // Ray tracing features
        if has_rt && has_as {
            let mut rt_features = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR::builder()
                .ray_tracing_pipeline(true)
                .build();
            let mut as_features = vk::PhysicalDeviceAccelerationStructureFeaturesKHR::builder()
                .acceleration_structure(true)
                .build();

            // Link feature chains
            features2.p_next = &mut rt_features as *mut _ as *mut std::ffi::c_void;
            rt_features.p_next = &mut as_features as *mut _ as *mut std::ffi::c_void;
        }

        let features = vk::PhysicalDeviceFeatures::builder()
            .sampler_anisotropy(true)
            .build();

        let info = vk::DeviceCreateInfo::builder()
            .queue_create_infos(&queue_create_infos)
            .enabled_extension_names(&device_exts.iter().map(|s| s.as_ptr()).collect::<Vec<*const i8>>())
            .enabled_layer_names(&[])
            .push_next(&features2)
            .build();

        let (device, _) = instance.create_device(physical, &info, None)
            .map_err(|e| format!("Failed to create device: {:?}", e))?;

        let draw_queue = device.queue(queue_families.graphics, 0);
        let compute_queue = if queue_families.compute != queue_families.graphics {
            device.queue(queue_families.compute, 0)
        } else {
            device.queue(queue_families.graphics, 0)
        };
        let transfer_queue = if queue_families.transfer != queue_families.graphics {
            device.queue(queue_families.transfer, 0)
        } else {
            device.queue(queue_families.graphics, 0)
        };

        Ok(Self {
            instance: instance.clone(),
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
            features,
            ext_features,
        })
    }

    /// Get memory type index for given properties
    pub fn memory_type_index(&self, type_filter: u32, properties: vk::MemoryPropertyFlags) -> Option<u32> {
        for i in 0..self.memory_properties.memory_type_count {
            if (type_filter & (1 << i)) != 0
                && self.memory_properties.memory_types[i as usize].property_flags.contains(properties)
            {
                return Some(i);
            }
        }
        None
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
