//! Minimal swapchain management (ash 0.38 idioms).

/// Owned swapchain bundle returned by [create_swapchain].
pub struct Swapchain {
    pub swapchain: vk::SwapchainKHR,
    pub images: Vec<vk::Image>,
    pub views: Vec<vk::ImageView>,
    pub extents: [u32; 3],
    pub format: vk::Format,
    pub image_count: u32,
}

/// Create a swapchain for a surface.
#[allow(clippy::too_many_arguments)]
pub fn create_swapchain(
    device: &ash::Device,
    physical: vk::PhysicalDevice,
    surface: vk::SurfaceKHR,
    queue_families: &crate::instance::QueueFamilies,
    surface_loader: &ash::khr::surface::Instance,
    swapchain_loader: &ash::khr::swapchain::Device,
    width: u32,
    height: u32,
) -> Result<Swapchain, String> {
    // Query surface capabilities
    let caps = unsafe {
        surface_loader
            .get_physical_device_surface_capabilities(physical, surface)
            .map_err(|e| format!("Failed to get surface capabilities: {e:?}"))?
    };

    // Determine image count
    let image_count = caps.min_image_count + 1;
    let image_count = if caps.max_image_count > 0 && image_count > caps.max_image_count {
        caps.max_image_count
    } else {
        image_count
    };

    // Find matching format
    let formats = unsafe {
        surface_loader
            .get_physical_device_surface_formats(physical, surface)
            .map_err(|e| format!("Failed to get surface formats: {e:?}"))?
    };
    let format = formats[0]; // Prefer first (usually RGBA8 or BGRA8)

    // Present mode
    let present_modes = unsafe {
        surface_loader
            .get_physical_device_surface_present_modes(physical, surface)
            .map_err(|e| format!("Failed to get present modes: {e:?}"))?
    };    let present_mode = if present_modes.contains(&vk::PresentModeKHR::MAILBOX) {
        vk::PresentModeKHR::MAILBOX
    } else if present_modes.contains(&vk::PresentModeKHR::FIFO) {
        vk::PresentModeKHR::FIFO
    } else {
        vk::PresentModeKHR::IMMEDIATE
    };

    // Determine image usage
    let usage = vk::ImageUsageFlags::COLOR_ATTACHMENT;
    let final_usage =
        if caps.supported_usage_flags.contains(vk::ImageUsageFlags::COLOR_ATTACHMENT) {
            usage
        } else {
            usage | vk::ImageUsageFlags::TRANSFER_DST
        };

    // Image sharing mode
    let sharing_mode = if queue_families.graphics == queue_families.compute {
        vk::SharingMode::EXCLUSIVE
    } else {
        vk::SharingMode::CONCURRENT
    };
    let queue_family_indices = if sharing_mode == vk::SharingMode::CONCURRENT {
        vec![queue_families.graphics, queue_families.compute]
    } else {
        vec![]
    };

    // Create swapchain
    let swapchain_info = vk::SwapchainCreateInfoKHR {
        surface,
        min_image_count: image_count,
        image_format: format.format,
        image_color_space: format.color_space,
        image_extent: vk::Extent2D { width, height },
        image_array_layers: 1,
        image_usage: final_usage,
        image_sharing_mode: sharing_mode,
        queue_family_index_count: queue_family_indices.len() as u32,
        p_queue_family_indices: queue_family_indices.as_ptr(),
        pre_transform: caps.current_transform,
        composite_alpha: vk::CompositeAlphaFlagsKHR::OPAQUE,
        present_mode,
        clipped: ash::vk::TRUE,
        old_swapchain: vk::SwapchainKHR::null(),
        ..Default::default()
    };

    let swapchain = unsafe {
        swapchain_loader
            .create_swapchain(&swapchain_info, None)
            .map_err(|e| format!("Failed to create swapchain: {e:?}"))?
    };

    // Get swapchain images
    let images = unsafe {
        swapchain_loader
            .get_swapchain_images(swapchain)
            .map_err(|e| format!("Failed to get swapchain images: {e:?}"))?
    };

    // Create image views
    let views: Vec<vk::ImageView> = images
        .iter()
        .map(|img| {
            let info = vk::ImageViewCreateInfo {
                image: *img,
                view_type: vk::ImageViewType::TYPE_2D,
                format: format.format,
                subresource_range: vk::ImageSubresourceRange {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    base_mip_level: 0,
                    level_count: 1,
                    base_array_layer: 0,
                    layer_count: 1,
                },
                ..Default::default()
            };
            unsafe { device.create_image_view(&info, None).unwrap() }
        })
        .collect();

    Ok(Swapchain {
        swapchain,
        images,
        views,
        extents: [width, height, 1],
        format: format.format,
        image_count,
    })
}

pub fn destroy_swapchain(
    device: &ash::Device,
    swapchain_loader: &ash::khr::swapchain::Device,
    swapchain: vk::SwapchainKHR,
    views: &[vk::ImageView],
) {
    for view in views {
        unsafe { device.destroy_image_view(*view, None) };
    }
    unsafe { swapchain_loader.destroy_swapchain(swapchain, None) };
}

/// Acquire the next swapchain image. Returns (image index, suboptimal).
#[allow(clippy::too_many_arguments)]
pub fn acquire_next_image(
    _device: &ash::Device,
    swapchain_loader: &ash::khr::swapchain::Device,
    swapchain: vk::SwapchainKHR,
    timeout: u64,
    semaphore: vk::Semaphore,
    fence: vk::Fence,
) -> Result<(u32, bool), vk::Result> {
    unsafe { swapchain_loader.acquire_next_image(swapchain, timeout, semaphore, fence) }
}

pub fn present(
    swapchain_loader: &ash::khr::swapchain::Device,
    swapchain: vk::SwapchainKHR,
    queue: vk::Queue,
    image_index: u32,
    wait_semaphore: vk::Semaphore,
) -> Result<bool, ()> {
    let wait_semaphores = [wait_semaphore];
    let swapchains = [swapchain];
    let image_indices = [image_index];
    let info = vk::PresentInfoKHR {
        wait_semaphore_count: wait_semaphores.len() as u32,
        p_wait_semaphores: wait_semaphores.as_ptr(),
        swapchain_count: swapchains.len() as u32,
        p_swapchains: swapchains.as_ptr(),
        p_image_indices: image_indices.as_ptr(),
        ..Default::default()
    };
    unsafe { swapchain_loader.queue_present(queue, &info).map_err(|e| { eprintln!("Present failed: {e:?}"); }) }
}

use ash::vk;
