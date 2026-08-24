//! Main renderer struct -- single render loop with full pipeline integration.

use ash::vk;
use litt_vulkan::*;
use litt_fidelityfx::Fsr3UpscalerConstants;
use super::*;

/// The main renderer with complete pipeline integration
pub struct Renderer {
    pub device: VulkanDevice,
    pub swapchain: Swapchain,
    pub command_pool: CommandPool,
    pub render_pass: RenderPass,
    pub descriptor_pool: DescriptorPool,
    /// Per-frame (fence, image_available) sync pairs; one slot per frame in flight.
    pub frames: Vec<(vk::Fence, vk::Semaphore)>,
    pub current_frame: u32,
    /// Complete render pipeline (FSR, BLAS/TLAS, CAS)
    pub render_pipeline: Option<RenderPipeline>,
}

impl Renderer {
    /// Create a new renderer around an existing logical device.
    pub fn new(device: VulkanDevice, window_size: (u32, u32)) -> Result<Self, String> {
        let queue_families = QueueFamilies {
            graphics: device.graphics_family,
            compute: device.compute_family,
            transfer: device.transfer_family,
            rt: device.compute_family,
        };

        let swapchain = create_swapchain(
            &device.device,
            device.physical_device,
            device.surface,
            &queue_families,
            &device.surface_loader,
            &device.swapchain_loader,
            window_size.0.max(1),
            window_size.1.max(1),
        )?;

        let command_pool = CommandPool::new(&device.device, device.graphics_family)?;
        let render_pass = RenderPass::new(&device.device, swapchain.format)?;
        let descriptor_pool = DescriptorPool::new(&device.device, 256)?;

        // Two frames in flight
        let frames = (0..2)
            .map(|_| unsafe {
                let fence_info = vk::FenceCreateInfo {
                    flags: vk::FenceCreateFlags::SIGNALED,
                    ..Default::default()
                };
                let fence = device
                    .device
                    .create_fence(&fence_info, None)
                    .map_err(|e| format!("Fence creation failed: {:?}", e))?;
                let sem_info = vk::SemaphoreCreateInfo::default();
                let available = device
                    .device
                    .create_semaphore(&sem_info, None)
                    .map_err(|e| format!("Semaphore creation failed: {:?}", e))?;
                Ok((fence, available))
            })
            .collect::<Result<Vec<_>, String>>()?;

        Ok(Self {
            device,
            swapchain,
            command_pool,
            render_pass,
            descriptor_pool,
            frames,
            current_frame: 0,
            render_pipeline: None,
        })
    }

    /// Initialize the complete render pipeline
    pub fn initialize_pipeline(
        &mut self,
        scene: litt_pathtracer::Scene,
        camera: litt_pathtracer::Camera,
    ) -> Result<(), String> {
        let rt_loader =
            ash::khr::acceleration_structure::Device::new(&self.device.instance, &self.device.device);

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

    /// Render a single frame -- path trace -> FSR upscale -> CAS sharpen -> present.
    ///
    /// One-shot command submission waits for queue idle before present, so no
    /// extra render-finished semaphore is required at this layer.
    pub fn render_frame(
        &mut self,
        scene: &litt_pathtracer::Scene,
        camera: &litt_pathtracer::Camera,
    ) -> Result<(), String> {
        let idx = self.current_frame as usize;
        let (fence, image_available) = self.frames[idx];

        unsafe {
            self.device
                .device
                .wait_for_fences(&[fence], true, u64::MAX)
                .map_err(|e| format!("Fence wait failed: {:?}", e))?;
            self.device
                .device
                .reset_fences(&[fence])
                .map_err(|e| format!("Reset fence failed: {:?}", e))?;
        }

        // Acquire next swapchain image
        let image_index = unsafe {
            acquire_next_image(
                &self.device.device,
                &self.device.swapchain_loader,
                self.swapchain.swapchain,
                u64::MAX,
                image_available,
                vk::Fence::null(),
            )
            .map_err(|e| format!("Acquire image failed: {:?}", e))?
            .0
        };

        // Begin command buffer recording
        let command_buffer = self.command_pool.begin_single_time_commands()?;

        // Update pipeline constants.
        // CRITICAL: the path tracer renders at the INTERNAL (FSR input)
        // resolution -- resolution_x/y in the push constants must match the
        // dispatch grid and accumulation buffer, or rays only cover the
        // top-left corner of the image. The accumulation buffer extent is
        // the single source of truth for that size.
        if let Some(ref mut pipeline) = self.render_pipeline {
            let iw = pipeline.path_tracer_buffers.accumulation_buffer.extent[0];
            let ih = pipeline.path_tracer_buffers.accumulation_buffer.extent[1];
            pipeline.update(camera, scene, iw, ih);
        }

        // Transition accumulation buffer to GENERAL for storage writes
        if let Some(ref pipeline) = self.render_pipeline {
            let acc_image = pipeline.path_tracer_buffers.accumulation_buffer.handle;
            if acc_image != vk::Image::null() {
                self.command_pool.transition_image_layout(
                    command_buffer,
                    acc_image,
                    vk::ImageAspectFlags::COLOR,
                    vk::ImageLayout::UNDEFINED,
                    vk::ImageLayout::GENERAL,
                    vk::PipelineStageFlags::TOP_OF_PIPE,
                    vk::PipelineStageFlags::COMPUTE_SHADER,
                    vk::AccessFlags::empty(),
                    vk::AccessFlags::SHADER_WRITE,
                )?;
            }
        }

        // GPU path trace (compute)
        if let Some(ref pipeline) = self.render_pipeline {
            if pipeline.path_trace_enabled && pipeline.path_tracer.is_initialized {
                let acc_view = pipeline.path_tracer_buffers.accumulation_buffer.view;
                if acc_view != vk::ImageView::null() {
                    let desc_set = unsafe { pipeline.allocate_path_tracer_desc_set()? };

                    let tc = pipeline.tracer_constants;
                    let pt_consts = PathTracePushConstants {
                        resolution_x: tc.resolution_x,
                        resolution_y: tc.resolution_y,
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

                    let tri = &pipeline.path_tracer_buffers.scene_triangles;
                    let sph = &pipeline.path_tracer_buffers.scene_spheres;
                    let lgt = &pipeline.path_tracer_buffers.scene_lights;
                    let mat = &pipeline.path_tracer_buffers.scene_materials;

                    let img_infos = [vk::DescriptorImageInfo {
                        sampler: vk::Sampler::null(),
                        image_view: acc_view,
                        image_layout: vk::ImageLayout::GENERAL,
                    }];
                    let buf_infos = [
                        [vk::DescriptorBufferInfo { buffer: tri.handle, offset: 0, range: tri.size }],
                        [vk::DescriptorBufferInfo { buffer: sph.handle, offset: 0, range: sph.size }],
                        [vk::DescriptorBufferInfo { buffer: lgt.handle, offset: 0, range: lgt.size }],
                        [vk::DescriptorBufferInfo { buffer: mat.handle, offset: 0, range: mat.size }],
                    ];
                    let writes = [
                        write_storage_buffer(desc_set, 0, &buf_infos[0]),
                        write_storage_buffer(desc_set, 1, &buf_infos[1]),
                        write_storage_buffer(desc_set, 2, &buf_infos[2]),
                        write_storage_buffer(desc_set, 3, &buf_infos[3]),
                        vk::WriteDescriptorSet {
                            dst_set: desc_set,
                            dst_binding: 4,
                            descriptor_count: 1,
                            descriptor_type: vk::DescriptorType::STORAGE_IMAGE,
                            p_image_info: img_infos.as_ptr(),
                            ..Default::default()
                        },
                    ];
                    unsafe { self.device.device.update_descriptor_sets(&writes, &[]); }

                    pipeline.path_tracer.dispatch(command_buffer, desc_set, &pt_consts)?;
                }
            }
        }

        // FSR 3.1.5 upscale + CAS sharpen into the swapchain image
        if let Some(ref pipeline) = self.render_pipeline {
            if pipeline.fsr_enabled && pipeline.fsr_pipeline.is_initialized {
                let acc_view = pipeline.path_tracer_buffers.accumulation_buffer.view;
                let swap_view = self.swapchain.views[image_index as usize];

                if acc_view != vk::ImageView::null() {
                    let fsr_consts = Fsr3UpscalerConstants {
                        input_width: pipeline.path_tracer_buffers.accumulation_buffer.extent[0],
                        input_height: pipeline.path_tracer_buffers.accumulation_buffer.extent[1],
                        output_width: pipeline.fsr_pipeline.output_width,
                        output_height: pipeline.fsr_pipeline.output_height,
                        ..Default::default()
                    };
                    let _ = pipeline.fsr_pipeline.run_upscaler(
                        command_buffer,
                        acc_view,
                        acc_view, // history = input on reset frames
                        pipeline.path_tracer_buffers.velocity_buffer.view,
                        swap_view,
                        &fsr_consts,
                    );

                    let cas_consts = CasConstants {
                        sharpening: pipeline.cas_constants.sharpening,
                        ..Default::default()
                    };
                    let _ =
                        pipeline.cas_pipeline.run(command_buffer, swap_view, swap_view, &cas_consts);
                }
            }
        }

        // Submit and wait for completion (single-queue one-shot model)
        self.command_pool
            .end_single_time_commands(command_buffer, self.device.draw_queue)?;

        // Present
        present(
            &self.device.swapchain_loader,
            self.swapchain.swapchain,
            self.device.draw_queue,
            image_index,
            image_available,
        )
        .map_err(|_| "Present failed".to_string())?;

        self.current_frame = (self.current_frame + 1) % 2;

        Ok(())
    }

    /// Resize the renderer
    pub fn resize(&mut self, width: u32, height: u32) -> Result<(), String> {
        unsafe {
            self.device
                .device
                .device_wait_idle()
                .map_err(|e| format!("Wait idle failed: {:?}", e))?;
        }

        destroy_swapchain(
            &self.device.device,
            &self.device.swapchain_loader,
            self.swapchain.swapchain,
            &self.swapchain.views,
        );

        let queue_families = QueueFamilies {
            graphics: self.device.graphics_family,
            compute: self.device.compute_family,
            transfer: self.device.transfer_family,
            rt: self.device.compute_family,
        };
        self.swapchain = create_swapchain(
            &self.device.device,
            self.device.physical_device,
            self.device.surface,
            &queue_families,
            &self.device.surface_loader,
            &self.device.swapchain_loader,
            width.max(1),
            height.max(1),
        )?;

        if let Some(ref mut pipeline) = self.render_pipeline {
            pipeline.tracer_constants.resolution_x = width;
            pipeline.tracer_constants.resolution_y = height;
        }

        Ok(())
    }
}

/// Helper: build a STORAGE_BUFFER write for a single buffer view.
fn write_storage_buffer(
    dst_set: vk::DescriptorSet,
    binding: u32,
    info: &[vk::DescriptorBufferInfo; 1],
) -> vk::WriteDescriptorSet {
    vk::WriteDescriptorSet {
        dst_set,
        dst_binding: binding,
        descriptor_count: 1,
        descriptor_type: vk::DescriptorType::STORAGE_BUFFER,
        p_buffer_info: info.as_ptr(),
        ..Default::default()
    }
}


