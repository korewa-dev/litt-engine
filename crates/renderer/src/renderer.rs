//! Main renderer struct — single render loop.
//! Minimal: no ECS, no scene graph, no editor.

use ash::{vk, Device};
use litt_vulkan::*;
use litt_math::*;

/// The main renderer
pub struct Renderer {
    pub device: VulkanDevice,
    pub swapchain: Swapchain,
    pub command_pool: CommandPool,
    pub render_pass: RenderPass,
    pub frame_in_flight: usize,
    pub fences: Vec<Fence>,
    pub semaphores: Vec<(Semaphore, Semaphore)>, // acquire, render
    pub descriptor_pool: DescriptorPool,
    pub current_frame: u32,
}

/// Push constants for the path tracer
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct PathTracerPushConstants {
    pub frame_count: u32,
    pub max_bounces: u32,
    pub resolution: u32,
    pub _pad: u32,
    pub camera_pos: Vec3,
    pub camera_yaw: f32,
    pub camera_pitch: f32,
    pub _pad2: f32,
    pub light_pos: Vec3,
    pub light_color: Vec3,
    pub light_intensity: f32,
}

/// Push constants for the display pipeline
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct DisplayPushConstants {
    pub exposure: f32,
    pub contrast: f32,
    pub gamma: f32,
    pub _pad: f32,
}

impl Renderer {
    pub unsafe fn new(
        instance: &ash::Instance,
        surface: vk::SurfaceKHR,
        window_size: (u32, u32),
    ) -> Result<Self, String> {
        let queue_families = find_queue_families(instance, instance.enumerate_physical_devices()
            .map_err(|e| format!("Enumerate failed: {:?}", e))?[0]).unwrap();

        let device = VulkanDevice::new(instance, instance.enumerate_physical_devices()
            .map_err(|e| format!("Enumerate failed: {:?}", e))?[0], surface, &queue_families)?;

        let swapchain = create_swapchain(
            &device.device,
            device.physical_device,
            surface,
            &queue_families,
            &device.surface_loader,
            &device.swapchain_loader,
            window_size.0,
            window_size.1,
        )?;

        let command_pool = CommandPool::new(&device.device, device.graphics_family)?;
        let render_pass = RenderPass::new(&device.device, swapchain.format)?;
        let descriptor_pool = DescriptorPool::new(&device.device, 256)?;

        let fences = (0..2).map(|_| Fence::new(&device.device)).collect();
        let semaphores = (0..2).map(|_| {
            (
                Semaphore::new(&device.device)?,
                Semaphore::new(&device.device)?,
            )
        }).collect::<Result<Vec<_>, _>>()?;

        Ok(Self {
            device,
            swapchain,
            command_pool,
            render_pass,
            frame_in_flight: 2,
            fences,
            semaphores,
            descriptor_pool,
            current_frame: 0,
        })
    }

    /// Resize the renderer
    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        unsafe { self.device.device.device_wait_idle()?; }

        // Destroy old swapchain
        destroy_swapchain(
            &self.device.device,
            &self.device.swapchain_loader,
            self.swapchain.swapchain,
            &self.swapchain.views,
        );

        // Create new swapchain
        self.swapchain = create_swapchain(
            &self.device.device,
            self.device.physical_device,
            self.device.surface,
            &QueueFamilies {
                graphics: self.device.graphics_family,
                compute: self.device.compute_family,
                transfer: self.device.transfer_family,
                rt: self.device.compute_family,
            },
            &self.device.surface_loader,
            &self.device.swapchain_loader,
            width,
            height,
        )?;

        Ok(())
    }
}
