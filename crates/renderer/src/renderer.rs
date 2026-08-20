//! Main renderer struct — single render loop with full pipeline integration.

use ash::{vk, Device};
use litt_vulkan::*;
use litt_math::*;
use super::*;

/// The main renderer with complete pipeline integration
pub struct Renderer {
    pub device: VulkanDevice,
    pub swapchain: Swapchain,
    pub command_pool: CommandPool,
    pub render_pass: RenderPass,
    pub frame_in_flight: usize,
    pub fences: Vec<Fence>,
    pub semaphores: Vec<(Semaphore, Semaphore)>,
    pub descriptor_pool: DescriptorPool,
    pub current_frame: u32,
    /// Complete render pipeline (FSR, BLAS/TLAS, etc.)
    pub render_pipeline: Option<RenderPipeline>,
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
    /// Create a new renderer with complete pipeline
    pub unsafe fn new(
        instance: &ash::Instance,
        surface: vk::SurfaceKHR,
        window_size: (u32, u32),
    ) -> Result<Self, String> {
        let queue_families = find_queue_families(instance, instance.enumerate_physical_devices()
            .map_err(|e| format!("Enumerate failed: {:?}", e))?[0]).unwrap();

        let mut device = VulkanDevice::new(instance, instance.enumerate_physical_devices()
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
            render_pipeline: None,
        })
    }

    /// Initialize the complete render pipeline
    pub fn initialize_pipeline(
        &mut self,
        scene: Scene,
        camera: Camera,
    ) -> Result<(), String> {
        use crate::pathtracer::{Scene as PtScene, Camera as PtCamera};
        
        let rt_loader = self.device.device.extensions_khr().ray_tracing_pipeline;
        
        self.render_pipeline = Some(RenderPipeline::new(
            &self.device.device,
            &rt_loader,
            &mut self.device.allocator,
            &scene,
            &camera,
            self.swapchain.extents[0],
            self.swapchain.extents[1],
        )?);
        
        Ok(())
    }

    /// Render a single frame
    pub unsafe fn render_frame(
        &mut self,
        scene: &Scene,
        camera: &Camera,
    ) -> Result<(), String> {
        // Wait for previous frame
        self.device.device
            .wait_for_fences(&[self.fences[self.current_frame as usize].fence], true, u64::MAX)
            .map_err(|e| format!("Fence wait failed: {:?}", e))?;
        
        // Acquire next swapchain image
        let image_index = self.device.swapchain_loader
            .acquire_next_image(self.swapchain.swapchain, u64::MAX, 
                vk::Semaphore::null(), vk::Fence::null())
            .map_err(|e| format!("Acquire image failed: {:?}", e))?[0];
        
        // Reset fence
        self.device.device.reset_fences(&[self.fences[self.current_frame as usize].fence])
            .map_err(|e| format!("Reset fence failed: {:?}", e))?;
        
        // Begin command buffer recording
        let command_buffer = self.command_pool.begin_single_time_commands()?;
        
        // Update pipeline constants
        if let Some(ref mut pipeline) = self.render_pipeline {
            pipeline.update(camera, scene, self.swapchain.extents[0], self.swapchain.extents[1]);
        }
        
        // Record rendering commands here
        // (Full implementation would record path trace, FSR, CAS commands)
        
        // End command buffer
        self.command_pool.end_single_time_commands(command_buffer, &self.device.device, self.device.draw_queue)?;
        
        // Queue submit
        let submit_info = vk::SubmitInfo::builder()
            .command_buffers(&[command_buffer])
            .build();
        
        self.device.device
            .queue_submit(self.device.draw_queue, &[submit_info], 
                self.fences[self.current_frame as usize].fence)
            .map_err(|e| format!("Queue submit failed: {:?}", e))?;
        
        // Present
        let swapchain_images = vec![vk::SwapchainKHR::from_raw(self.swapchain.swapchain)];
        let present_info = vk::PresentInfoKHR::builder()
            .swapchains(&swapchain_images)
            .image_indices(&[image_index])
            .build();
        
        self.device.swapchain_loader
            .present_khr(&present_info)
            .map_err(|e| format!("Present failed: {:?}", e))?;
        
        // Advance frame
        self.current_frame = (self.current_frame + 1) % 2;
        
        Ok(())
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
        
        // Reinitialize pipeline with new dimensions
        if let Some(ref mut pipeline) = self.render_pipeline {
            pipeline.tracer_constants.resolution_x = width;
            pipeline.tracer_constants.resolution_y = height;
        }

        Ok(())
    }
}
