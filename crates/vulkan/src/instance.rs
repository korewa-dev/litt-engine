//! Vulkan instance creation with AMD GPU selection.
//! Targets Vulkan 1.3 with RT and FidelityFX extensions.

use ash::{vk, extensions::khr, Instance};
use super::*;

/// Required Vulkan extensions for all platforms
/// Supports: AMD (RADV/AMDVLK), Moore Threads (MUSA), Intel (Arc/DDR Xe)
const REQUIRED_EXTENSIONS: &[&str] = &[
    khr::Surface::NAME.as_ptr() as *const u8 as &str,
    khr::Swapchain::NAME.as_ptr() as *const u8 as &str,
];

/// Required Vulkan 1.3 instance extensions
#[cfg(target_os = "windows")]
pub const INSTANCE_EXTENSIONS: &[&str] = &[
    "VK_KHR_win32_surface",
    "VK_KHR_get_physical_device_properties2",
];

#[cfg(target_os = "linux")]
pub const INSTANCE_EXTENSIONS: &[&str] = &[
    "VK_KHR_xlib_surface",
    "VK_KHR_xcb_surface",
    "VK_KHR_wayland_surface",
    "VK_KHR_get_physical_device_properties2",
];

#[cfg(target_os = "android")]
pub const INSTANCE_EXTENSIONS: &[&str] = &[
    "VK_KHR_android_surface",
    "VK_KHR_get_physical_device_properties2",
];

/// Required instance extensions for ray tracing
pub const RT_INSTANCE_EXTENSIONS: &[&str] = &[
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_acceleration_structure",
    "VK_KHR_deferred_host_operations",
    "VK_EXT_descriptor_indexing",
    "VK_KHR_spirv_1_4",
];

/// AMD-specific features for optimization
pub const AMD_FEATURES: &[&str] = &[
    "VK_AMD_shader_core_properties",
    "VK_AMD_shader_info",
    "VK_EXT_shader_subgroup_extended_types",
    "VK_EXT_descriptor_indexing",
    "VK_KHR_ray_tracing_pipeline",
    "VK_KHR_acceleration_structure",
    "VK_KHR_deferred_host_operations",
    "VK_EXT_pipeline_creation_cache_control",
];

/// Select the best AMD GPU on the system
pub fn enumerate_adapters(
    instance: &Instance,
) -> Result<Vec<vk::PhysicalDevice>, String> {
    let devices = unsafe {
        instance.enumerate_physical_devices()
            .map_err(|e| format!("Failed to enumerate physical devices: {:?}", e))?
    };

    let mut amd_devices: Vec<vk::PhysicalDevice> = Vec::new();
    let mut other_devices: Vec<vk::PhysicalDevice> = Vec::new();

    for device in devices {
        let info = unsafe { instance.physical_device_properties(device) };
        let vendor_id = info.vendor_id;

        // AMD vendor ID
        if vendor_id == 0x1002 {
            amd_devices.push(device);
        // Moore Threads vendor ID (0x1DD = 573)
        } else if vendor_id == 0x1DD {
            amd_devices.push(device);
        // Intel vendor ID (0x8086)
        } else if vendor_id == 0x8086 {
            amd_devices.push(device);
        } else {
            other_devices.push(device);
        }
    }

    // Prefer AMD devices for optimization
    if !amd_devices.is_empty() {
        Ok(amd_devices)
    } else if !other_devices.is_empty() {
        Ok(other_devices)
    } else {
        Err("No Vulkan-compatible GPU found".to_string())
    }
}

/// Check if a physical device supports our requirements
pub fn is_compatible(
    instance: &Instance,
    device: vk::PhysicalDevice,
    required_extensions: &[&str],
) -> bool {
    let props = unsafe { instance.physical_device_properties(device) };
    let mem_props = unsafe { instance.physical_device_memory_properties(device) };
    let queue_props = unsafe { instance.physical_device_queue_family_properties(device) };

    // Check Vulkan version (need 1.2+ for RT)
    if props.api_version < vk::make_api_version(0, 1, 2, 0) {
        return false;
    }

    // Check device extensions
    let device_extensions = unsafe {
        instance.enumerate_device_extension_properties(device)
            .unwrap_or_default()
    };

    for ext in required_extensions {
        let ext_name = std::ffi::CStr::from_bytes_with_nul(ext.as_bytes()).unwrap();
        if !device_extensions.iter().any(|e| {
            unsafe {
                std::ffi::CStr::from_ptr(e.extension_name.as_ptr()) == ext_name
            }
        }) {
            return false;
        }
    }

    // Check queue families
    let has_graphics = queue_props.iter().any(|q| {
        q.queue_flags.contains(vk::QueueFlags::GRAPHICS)
    });

    let has_compute = queue_props.iter().any(|q| {
        q.queue_flags.contains(vk::QueueFlags::COMPUTE)
    });

    has_graphics && has_compute
}

/// Find queue families for graphics, compute, and transfer
pub fn find_queue_families(
    instance: &Instance,
    device: vk::PhysicalDevice,
) -> Result<QueueFamilies, String> {
    let queue_props = unsafe {
        instance.physical_device_queue_family_properties(device)
    };

    let mut graphics = None;
    let mut compute = None;
    let mut transfer = None;
    let mut rt = None;

    for (i, props) in queue_props.iter().enumerate() {
        let flags = props.queue_flags;
        if flags.contains(vk::QueueFlags::GRAPHICS) && graphics.is_none() {
            graphics = Some(i as u32);
        }
        if flags.contains(vk::QueueFlags::COMPUTE) && compute.is_none() {
            compute = Some(i as u32);
        }
        if flags.contains(vk::QueueFlags::TRANSFER) && transfer.is_none() {
            transfer = Some(i as u32);
        }
        // RT often shares with compute
        if flags.contains(vk::QueueFlags::COMPUTE) && rt.is_none() {
            rt = Some(i as u32);
        }
    }

    Ok(QueueFamilies {
        graphics: graphics.unwrap_or(0),
        compute: compute.unwrap_or(0),
        transfer: transfer.unwrap_or(0),
        rt: rt.unwrap_or(0),
    })
}
