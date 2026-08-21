//! Main renderer struct — single render loop with full pipeline integration.

use ash::{vk, Device};
use litt_vulkan::*;
use litt_math::*;
use super::*;
use crate::fidelityfx::DisplayPipeline;

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
    /// Complete render pipeline (FSR, BLAS/TLAS, CAS)
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
        let phy_devs = instance.enumerate_physical_devices()
            .map_err(|e| format!("Enumerate failed: {:?}", e))?;
        let phy_dev = phy_devs[0];
        let queue_families = find_queue_families(instance, phy_dev).ok_or("No queue families")?;

        let mut device = VulkanDevice::new(instance, phy_dev, surface, &queue_families)?;

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

    /// Render a single frame — path trace → FSR upscaler → CAS sharpen → present
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

        // ── GPU Path Trace (compute) ──────────────────────────────────────
        if let Some(ref pipeline) = self.render_pipeline {
            if pipeline.path_trace_enabled && pipeline.path_tracer.is_initialized {
                let acc_view = pipeline.path_tracer_buffers.accumulation_buffer.view;
                if acc_view != vk::ImageView::null() {
                    // Allocate descriptor set for path tracer
                    let desc_set = unsafe {
                        pipeline.allocate_path_tracer_desc_set()?
                    };

                    // Build push constants from tracer_constants
                    let tc = &pipeline.tracer_constants;
                    let pt_consts = PathTracePushConstants {
                        resolution_x: pipeline.path_tracer.width,
                        resolution_y: pipeline.path_tracer.height,
                        max_bounces: tc.max_bounces,
                        frame_count: pipeline.frame_count,
                        camera_pos_x: tc.camera_pos_x,
                        camera_pos_y: tc.camera_pos_y,
                        camera_pos_z: tc.camera_pos_z,
                        camera_yaw: tc.camera_yaw,
                        camera_pitch: tc.camera_pitch,
                        fov: tc.fov,
                        aspect: tc.aspect,
                        light_count: tc.light_count,
                        ..Default::default()
                    };

                    // Bind scene buffers to descriptor set
                    let writes = [
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 0, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_BUFFER,
                            p_buffer_info: &[vk::DescriptorBufferInfo {
                                buffer: pipeline.path_tracer_buffers.scene_triangles.handle,
                                offset: 0,
                                range: pipeline.path_tracer_buffers.scene_triangles.size,
                            }],
                            ..Default::default()
                        },
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 1, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_BUFFER,
                            p_buffer_info: &[vk::DescriptorBufferInfo {
                                buffer: pipeline.path_tracer_buffers.scene_spheres.handle,
                                offset: 0,
                                range: pipeline.path_tracer_buffers.scene_spheres.size,
                            }],
                            ..Default::default()
                        },
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 2, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_BUFFER,
                            p_buffer_info: &[vk::DescriptorBufferInfo {
                                buffer: pipeline.path_tracer_buffers.scene_lights.handle,
                                offset: 0,
                                range: pipeline.path_tracer_buffers.scene_lights.size,
                            }],
                            ..Default::default()
                        },
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 3, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_BUFFER,
                            p_buffer_info: &[vk::DescriptorBufferInfo {
                                buffer: pipeline.path_tracer_buffers.scene_materials.handle,
                                offset: 0,
                                range: pipeline.path_tracer_buffers.scene_materials.size,
                            }],
                            ..Default::default()
                        },
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 4, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_IMAGE,
                            p_image_info: &[vk::DescriptorImageInfo {
                                sampler: vk::Sampler::null(),
                                image_view: acc_view,
                                image_layout: vk::ImageLayout::GENERAL,
                            }],
                            ..Default::default()
                        },
                    ];
                    unsafe { self.device.device.update_descriptor_sets(&writes, &[]); }

                    // Dispatch path trace
                    pipeline.path_tracer.dispatch(
                        command_buffer,
                        desc_set,
                        &pt_consts,
                    )?;
                }
            }
        }

        // ── FSR 3.1.5 Upscale + CAS Sharpen ─────────────────────────────
        if let Some(ref pipeline) = self.render_pipeline {
            if pipeline.fsr_enabled && pipeline.fsr_pipeline.is_initialized {
                let acc_view = pipeline.path_tracer_buffers.accumulation_buffer.view;
                let swap_view = self.swapchain.views[image_index as usize];

                if acc_view != vk::ImageView::null() {
                    // FSR upscaler: low-res path trace → high-res swapchain
                    let fsr_consts = Fsr3UpscalerConstants {
                        input_width: pipeline.path_tracer_buffers.accumulation_buffer.extent[0],
                        input_height: pipeline.path_tracer_buffers.accumulation_buffer.extent[1],
                        output_width: pipeline.fsr_pipeline.output_width,
                        output_height: pipeline.fsr_pipeline.output_height,
                        ..Default::default()
                    };
                    pipeline.fsr_pipeline.run_upscaler(
                        command_buffer,
                        acc_view,
                        acc_view, // history = input for first frame
                        pipeline.path_tracer_buffers.velocity_buffer.view,
                        swap_view,
                        &fsr_consts,
                    ).ok();

                    // CAS sharpening
                    let cas_consts = CasConstants {
                        sharpening: pipeline.cas_constants.sharpening,
                        ..Default::default()
                    };
                    pipeline.cas_pipeline.run(
                        command_buffer,
                        swap_view,
                        swap_view,
                        &cas_consts,
                    ).ok();
                }
            }
        }

        // ── Display / Tone-Map Pass ───────────────────────────────────────
        if let Some(ref pipeline) = self.render_pipeline {
            if pipeline.display_pipeline.is_initialized {
                let acc_view = pipeline.path_tracer_buffers.accumulation_buffer.view;
                let swap_view = self.swapchain.views[image_index as usize];
                if acc_view != vk::ImageView::null() && swap_view != vk::ImageView::null() {
                    let desc_set = unsafe {
                        pipeline.display_pipeline.allocate_descriptor_set()?
                    };
                    let dp_consts = DisplayPushConstants {
                        resolution_x: pipeline.display_pipeline.width,
                        resolution_y: pipeline.display_pipeline.height,
                        ..Default::default()
                    };

                    let writes = [
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 0, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_IMAGE,
                            p_image_info: &[vk::DescriptorImageInfo {
                                sampler: vk::Sampler::null(),
                                image_view: acc_view,
                                image_layout: vk::ImageLayout::GENERAL,
                            }],
                            ..Default::default()
                        },
                        vk::WriteDescriptorSet {
                            dst_set: desc_set, dst_binding: 1, descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_IMAGE,
                            p_image_info: &[vk::DescriptorImageInfo {
                                sampler: vk::Sampler::null(),
                                image_view: swap_view,
                                image_layout: vk::ImageLayout::GENERAL,
                            }],
                            ..Default::default()
                        },
                    ];
                    unsafe { self.device.device.update_descriptor_sets(&writes, &[]); }

                    pipeline.display_pipeline.dispatch(
                        command_buffer,
                        desc_set,
                        &dp_consts,
                    )?;
                }
            }
        }

        // Transition swapchain image to present layout
        // (VkImageLayout::PRESENT_SRC_KHR)
        // The render_pass and semaphore handling in command_pool covers this.

        // End command buffer
        self.command_pool.end_single_time_commands(command_buffer, &self.device.device, self.device.draw_queue)?;

        // Queue submit with semaphore signaling
        let wait_semaphores = &[self.semaphores[self.current_frame as usize].0.semaphore];
        let signal_semaphores = &[self.semaphores[self.current_frame as usize].1.semaphore];
        let submit_info = vk::SubmitInfo::builder()
            .wait_semaphores(wait_semaphores)
            .signal_semaphores(signal_semaphores)
            .command_buffers(&[command_buffer])
            .build();

        self.device.device
            .queue_submit(self.device.draw_queue, &[submit_info],
                self.fences[self.current_frame as usize].fence)
            .map_err(|e| format!("Queue submit failed: {:?}", e))?;

        // Present
        let present_info = vk::PresentInfoKHR::builder()
            .wait_semaphores(signal_semaphores)
            .swapchains(&[self.swapchain.swapchain])
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
