//! Graphics backend abstraction -- Vulkan or DX12
//!
//! Selects the appropriate graphics backend at runtime based on platform
//! and availability. Vulkan is primary; DX12 is the Windows-native path.

/// Graphics backend feature flags
#[derive(Clone, Debug, Default)]
pub struct GraphicsFeatures {
    pub ray_tracing: bool,
    pub mesh_shader: bool,
    pub variable_rate_shading: bool,
    pub acceleration_structure: bool,
}

/// DX12 feature levels
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FeatureLevel {
    DX12_1,
    DX12_2,
    DX12_3,
    DX12_4,
    DX12_5,
    DX12_6,
    DX12_7,
    DX12_8,
    DX12_9,
    DX12_10,
    DX12_11,
    DX12_12,
}

/// DX12 specific features
#[derive(Clone, Debug, Default)]
pub struct Dx12Features {
    pub mesh_shader: bool,
    pub raytracing: bool,
    pub variable_rate_shading: bool,
    pub sampler_feedback: bool,
}

/// Graphics backend trait
pub trait GraphicsBackend: Send + Sync {
    /// Get the backend name
    fn name(&self) -> &str;

    /// Hand the backend a native window handle before initialize().
    /// No-op for backends that do not need one.
    fn set_window(&mut self, _hwnd: isize) {}

    /// Check if ray tracing is supported
    fn supports_ray_tracing(&self) -> bool;

    /// Check if mesh shaders are supported
    fn supports_mesh_shaders(&self) -> bool;

    /// Get the adapter info
    fn adapter_info(&self) -> &str;

    /// Initialize the backend (after window creation)
    fn initialize(&mut self, width: u32, height: u32) -> Result<(), String>;

    /// Begin a new frame
    fn begin_frame(&mut self) -> Result<(), String>;

    /// Record render commands
    fn render(&mut self, scene: &litt_pathtracer::Scene, camera: &litt_pathtracer::Camera) -> Result<(), String>;

    /// Present the frame
    fn present(&mut self) -> Result<(), String>;

    /// End the frame
    fn end_frame(&mut self) -> Result<(), String>;

    /// Shutdown the backend
    fn shutdown(&mut self) -> Result<(), String>;

    /// Upload world geometry as interleaved xyzrgb f32 triples (Studio).
    /// No-op on backends without the Studio pipeline.
    fn upload_world_mesh(&mut self, _verts: &[f32]) {}

    /// Upload chat-panel geometry in pixel space (Studio).
    fn upload_panel_mesh(&mut self, _verts: &[f32]) {}

    /// Set the world view-projection matrix (Studio orbit camera).
    fn set_world_mvp(&mut self, _mvp: [f32; 16]) {}

    /// Whether Studio drawing is live (pipelines + buffers ready).
    fn studio_ready(&self) -> bool { false }
}

/// Width of the Studio chat panel (px). The 3D viewport uses the rest.
pub const STUDIO_PANEL_W: u32 = 430;

/// Vulkan backend wrapper
#[cfg(feature = "vulkan")]
pub mod vulkan {
    use super::*;
    use ash::vk;
    use litt_vulkan::{
        create_swapchain, destroy_swapchain, acquire_next_image, present as vk_present,
        enumerate_adapters, find_queue_families, Swapchain, VulkanDevice,
    };
    use litt_renderer::{CommandPool, RenderPass};
    use std::sync::Arc;
    use std::time::Instant;

    /// A real, presenting Vulkan backend: instance -> surface -> device ->
    /// swapchain -> render pass -> command buffer cycle. Mesh geometry upload
    /// is the next milestone; frames currently clear with a world-derived
    /// tone so the swapchain path is exercised end-to-end.
    pub struct VulkanBackend {
        hwnd: isize,
        entry: Option<Arc<ash::Entry>>,
        instance: Option<ash::Instance>,
        surface: vk::SurfaceKHR,
        physical: vk::PhysicalDevice,
        device: Option<VulkanDevice>,
        swapchain: Option<Swapchain>,
        render_pass: Option<RenderPass>,
        framebuffers: Vec<vk::Framebuffer>,
        command_pool: Option<CommandPool>,
        cmd: Option<vk::CommandBuffer>,
        image_available: vk::Semaphore,
        render_done: vk::Semaphore,
        in_flight: vk::Fence,
        frame_index: u32,
        just_recreated: bool,
        width: u32,
        height: u32,
        started: Instant,
        gpu_name: String,
        features: GraphicsFeatures,
        // ---- Studio (chat panel + world mesh) ----
        allocator: Option<litt_vulkan::GpuAllocator>,
        studio_pipe: vk::Pipeline,
        studio_layout: Option<vk::PipelineLayout>,
        world_buf: Option<(vk::Buffer, litt_vulkan::Allocation, u64)>,
        world_count: u32,
        ui_buf: Option<(vk::Buffer, litt_vulkan::Allocation, u64)>,
        ui_count: u32,
        world_mvp: [f32; 16],
    }

    /// Reinterpret a static byte blob as SPIR-V words (4-byte aligned).
    fn spv_words(bytes: &'static [u8]) -> &'static [u32] {
        debug_assert_eq!(bytes.len() % 4, 0);
        unsafe { std::slice::from_raw_parts(bytes.as_ptr() as *const u32, bytes.len() / 4) }
    }

    /// Host-visible buffer that grows geometrically; writes go through the
    /// persistent mapping (HOST_VISIBLE | HOST_COHERENT).
    fn ensure_host_buffer(
        dev: &VulkanDevice,
        alloc: &mut litt_vulkan::GpuAllocator,
        slot: &mut Option<(vk::Buffer, litt_vulkan::Allocation, u64)>,
        needed: u64,
    ) -> Result<(), String> {
        let cap = slot.as_ref().map(|s| s.2).unwrap_or(0);
        if needed <= cap {
            return Ok(());
        }
        let new_cap = (needed as f64 * 1.5) as u64;
        if let Some((buf, mut a, _)) = slot.take() {
            alloc.free_buffer(buf, &mut a);
        }
        let (buf, mut a) = alloc.allocate_buffer(
            new_cap,
            vk::BufferUsageFlags::VERTEX_BUFFER,
            litt_vulkan::AllocFlags::HOST_VISIBLE,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            vk::MemoryPropertyFlags::empty(),
        )?;
        *slot = Some((buf, a, new_cap));
        let _ = dev; // device reachable through allocator
        Ok(())
    }

    fn write_host_buffer(slot: &mut Option<(vk::Buffer, litt_vulkan::Allocation, u64)>, data: &[f32]) {
        if let Some((_, alloc, _)) = slot {
            if !alloc.mapped.is_null() {
                let bytes = unsafe {
                    std::slice::from_raw_parts_mut(alloc.mapped as *mut u8, data.len() * 4)
                };
                bytes.copy_from_slice(unsafe {
                    std::slice::from_raw_parts(data.as_ptr() as *const u8, data.len() * 4)
                });
            }
        }
    }

    impl VulkanBackend {
        pub fn new() -> Self {
            Self {
                hwnd: 0,
                entry: None,
                instance: None,
                surface: vk::SurfaceKHR::null(),
                physical: vk::PhysicalDevice::null(),
                device: None,
                swapchain: None,
                render_pass: None,
                framebuffers: Vec::new(),
                command_pool: None,
                cmd: None,
                image_available: vk::Semaphore::null(),
                render_done: vk::Semaphore::null(),
                in_flight: vk::Fence::null(),
                frame_index: 0,
                just_recreated: false,
                width: 0,
                height: 0,
                started: Instant::now(),
                gpu_name: String::new(),
                features: GraphicsFeatures::default(),
                allocator: None,
                studio_pipe: vk::Pipeline::null(),
                studio_layout: None,
                world_buf: None,
                world_count: 0,
                ui_buf: None,
                ui_count: 0,
                world_mvp: [0.0; 16],
            }
        }

        #[cfg(target_os = "windows")]
        fn module_handle() -> isize {
            // windows-sys is a direct dependency of this crate's package
            unsafe { windows_sys::Win32::System::LibraryLoader::GetModuleHandleW(std::ptr::null()) as isize }
        }

        #[cfg(target_os = "windows")]
        fn create_instance(entry: &Arc<ash::Entry>) -> Result<ash::Instance, String> {
            let exts = [
                b"VK_KHR_surface\0".as_ptr().cast(),
                b"VK_KHR_win32_surface\0".as_ptr().cast(),
            ];
            let info = vk::InstanceCreateInfo {
                enabled_extension_count: exts.len() as u32,
                pp_enabled_extension_names: exts.as_ptr(),
                ..Default::default()
            };
            unsafe { entry.create_instance(&info, None) }
                .map_err(|e| format!("Instance creation failed: {e:?}"))
        }

        #[cfg(not(target_os = "windows"))]
        fn create_instance(_entry: &Arc<ash::Entry>) -> Result<ash::Instance, String> {
            Err("non-Windows surface creation not wired yet".into())
        }

        fn create_framebuffers(&mut self) -> Result<(), String> {
            let dev = self.device.as_ref().ok_or("no device")?;
            let sc = self.swapchain.as_ref().ok_or("no swapchain")?;
            let pass = self.render_pass.as_ref().ok_or("no render pass")?.pass;
            let mut fbs = Vec::with_capacity(sc.views.len());
            for view in &sc.views {
                let attachments = [*view];
                let info = vk::FramebufferCreateInfo {
                    render_pass: pass,
                    attachment_count: 1,
                    p_attachments: attachments.as_ptr(),
                    width: sc.extents[0],
                    height: sc.extents[1],
                    layers: 1,
                    ..Default::default()
                };
                let fb = unsafe { dev.device.create_framebuffer(&info, None) }
                    .map_err(|e| format!("Framebuffer failed: {e:?}"))?;
                fbs.push(fb);
            }
            self.framebuffers = fbs;
            Ok(())
        }

        fn recreate_swapchain(&mut self) -> Result<(), String> {
            let dev = self.device.as_ref().ok_or("no device")?;
            if let Some(old) = self.swapchain.take() {
                destroy_swapchain(&dev.device, &dev.swapchain_loader, old.swapchain, &old.views);
            }
            for fb in self.framebuffers.drain(..) {
                unsafe { dev.device.destroy_framebuffer(fb, None) };
            }
            let families = find_queue_families(&dev.instance, dev.physical_device)?;
            let sc = create_swapchain(
                &dev.device,
                dev.physical_device,
                self.surface,
                &families,
                &dev.surface_loader,
                &dev.swapchain_loader,
                self.width.max(1),
                self.height.max(1),
            )?;
            self.swapchain = Some(sc);
            self.create_framebuffers()?;
            Ok(())
        }
    }

    impl Default for VulkanBackend {
        fn default() -> Self { Self::new() }
    }

    impl GraphicsBackend for VulkanBackend {
        fn name(&self) -> &str { "Vulkan" }
        fn supports_ray_tracing(&self) -> bool { self.features.ray_tracing }
        fn supports_mesh_shaders(&self) -> bool { false }
        fn adapter_info(&self) -> &str { &self.gpu_name }
        fn set_window(&mut self, hwnd: isize) { self.hwnd = hwnd; }

        fn initialize(&mut self, width: u32, height: u32) -> Result<(), String> {
            self.width = width;
            self.height = height;

            // Known-broken implicit layers inject into every Vulkan app on
            // this machine (TikTok LIVE Studio hook + AMD switchable
            // graphics). Both fail vkCreateInstance outright; disabling them
            // here affects ONLY this process.
            #[cfg(target_os = "windows")]
            {
                std::env::set_var("DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1", "1");
                std::env::set_var("DISABLE_VK_LAYER_MEDIASDK_HOOK_1", "1");
            }

            unsafe {
                let entry = Arc::new(ash::Entry::load()
                    .map_err(|e| format!("Vulkan loader unavailable: {e:?}"))?);
                let instance = Self::create_instance(&entry)?;

                // Surface from the platform window
                let surface = if self.hwnd != 0 {
                    #[cfg(target_os = "windows")]
                    {
                        let w32 = ash::khr::win32_surface::Instance::new(&entry, &instance);
                        let info = vk::Win32SurfaceCreateInfoKHR {
                            hinstance: Self::module_handle(),
                            hwnd: self.hwnd,
                            ..Default::default()
                        };
                        w32.create_win32_surface(&info, None)
                            .map_err(|e| format!("Win32 surface failed: {e:?}"))?
                    }
                    #[cfg(not(target_os = "windows"))]
                    { vk::SurfaceKHR::null() }
                } else {
                    return Err("VulkanBackend needs a window (set_window)".into());
                };

                // Physical device selection
                let adapters = enumerate_adapters(&instance)?;
                let mut chosen = None;
                for phys in &adapters {
                    let families = find_queue_families(&instance, *phys)?;
                    let props = instance.get_physical_device_properties(*phys);
                    if VulkanDevice::new(&entry, &instance, *phys, surface, &families).is_ok() {
                        chosen = Some((*phys, families, props));
                        break;
                    }
                }
                let (physical, families, props) =
                    chosen.ok_or_else(|| "no compatible Vulkan device".to_string())?;
                self.gpu_name = String::from_utf8_lossy(
                    &props.device_name.iter().map(|&c| c as u8).collect::<Vec<_>>(),
                )
                .trim_end_matches('\0')
                .to_string();

                let device = VulkanDevice::new(&entry, &instance, physical, surface, &families)?;

                // Feature flags from what actually came up
                self.features.ray_tracing = device.ext_features.ray_tracing;
                self.features.acceleration_structure = device.ext_features.acceleration_structure;

                let swapchain = create_swapchain(
                    &device.device,
                    device.physical_device,
                    surface,
                    &families,
                    &device.surface_loader,
                    &device.swapchain_loader,
                    width.max(1),
                    height.max(1),
                )?;

                let pass = RenderPass::new(&device.device, swapchain.format)?;
                let pool = CommandPool::new(&device.device, device.graphics_family)?;

                let sem_info = vk::SemaphoreCreateInfo::default();
                let image_available = device.device.create_semaphore(&sem_info, None)
                    .map_err(|e| format!("semaphore: {e:?}"))?;
                let render_done = device.device.create_semaphore(&sem_info, None)
                    .map_err(|e| format!("semaphore: {e:?}"))?;
                let fence_info = vk::FenceCreateInfo {
                    flags: vk::FenceCreateFlags::SIGNALED,
                    ..Default::default()
                };
                let in_flight = device.device.create_fence(&fence_info, None)
                    .map_err(|e| format!("fence: {e:?}"))?;

                let alloc_info = vk::CommandBufferAllocateInfo {
                    command_pool: pool.pool,
                    level: vk::CommandBufferLevel::PRIMARY,
                    command_buffer_count: 1,
                    ..Default::default()
                };
                let cmd = device.device.allocate_command_buffers(&alloc_info)
                    .map_err(|e| format!("cmd alloc: {e:?}"))?[0];

                // ---- Studio pipeline + memory allocator ----
                let mut allocator = litt_vulkan::GpuAllocator::new(
                    &device.device, physical, &instance,
                )?;

                #[cfg(litt_studio_spv)]
                {
                    let vert_spv: &[u32] = spv_words(include_bytes!(
                        concat!(env!("OUT_DIR"), "/spirv/studio_vert.spv")));
                    let frag_spv: &[u32] = spv_words(include_bytes!(
                        concat!(env!("OUT_DIR"), "/spirv/studio_frag.spv")));

                    let vmod = device.device
                        .create_shader_module(&vk::ShaderModuleCreateInfo {
                            code_size: vert_spv.len() * 4,
                            p_code: vert_spv.as_ptr(),
                            ..Default::default()
                        }, None)
                        .map_err(|e| format!("studio vert module: {e:?}"))?;
                    let fmod = device.device
                        .create_shader_module(&vk::ShaderModuleCreateInfo {
                            code_size: frag_spv.len() * 4,
                            p_code: frag_spv.as_ptr(),
                            ..Default::default()
                        }, None)
                        .map_err(|e| format!("studio frag module: {e:?}"))?;

                    let pc_range = vk::PushConstantRange {
                        stage_flags: vk::ShaderStageFlags::VERTEX,
                        offset: 0,
                        size: 64, // mat4
                    };
                    let layout_info = vk::PipelineLayoutCreateInfo {
                        push_constant_range_count: 1,
                        p_push_constant_ranges: &pc_range,
                        ..Default::default()
                    };
                    let layout = device.device
                        .create_pipeline_layout(&layout_info, None)
                        .map_err(|e| format!("studio layout: {e:?}"))?;

                    let stages = [
                        vk::PipelineShaderStageCreateInfo {
                            stage: vk::ShaderStageFlags::VERTEX,
                            module: vmod,
                            p_name: b"main\0".as_ptr().cast(),
                            ..Default::default()
                        },
                        vk::PipelineShaderStageCreateInfo {
                            stage: vk::ShaderStageFlags::FRAGMENT,
                            module: fmod,
                            p_name: b"main\0".as_ptr().cast(),
                            ..Default::default()
                        },
                    ];
                    let binding = vk::VertexInputBindingDescription {
                        binding: 0,
                        stride: 24, // pos(3f32) + color(3f32)
                        input_rate: vk::VertexInputRate::VERTEX,
                    };
                    let attribs = [
                        vk::VertexInputAttributeDescription {
                            location: 0, binding: 0,
                            format: vk::Format::R32G32B32_SFLOAT, offset: 0,
                        },
                        vk::VertexInputAttributeDescription {
                            location: 1, binding: 0,
                            format: vk::Format::R32G32B32_SFLOAT, offset: 12,
                        },
                    ];
                    let vertex_input = vk::PipelineVertexInputStateCreateInfo {
                        vertex_binding_description_count: 1,
                        p_vertex_binding_descriptions: &binding,
                        vertex_attribute_description_count: attribs.len() as u32,
                        p_vertex_attribute_descriptions: attribs.as_ptr(),
                        ..Default::default()
                    };
                    let asm = vk::PipelineInputAssemblyStateCreateInfo {
                        topology: vk::PrimitiveTopology::TRIANGLE_LIST,
                        ..Default::default()
                    };
                    let viewports = [vk::Viewport::default()];
                    let scissors = [vk::Rect2D::default()];
                    let viewport_state = vk::PipelineViewportStateCreateInfo {
                        viewport_count: 1,
                        p_viewports: viewports.as_ptr(),
                        scissor_count: 1,
                        p_scissors: scissors.as_ptr(),
                        ..Default::default()
                    };
                    let raster = vk::PipelineRasterizationStateCreateInfo {
                        polygon_mode: vk::PolygonMode::FILL,
                        cull_mode: vk::CullModeFlags::NONE,
                        front_face: vk::FrontFace::COUNTER_CLOCKWISE,
                        line_width: 1.0,
                        ..Default::default()
                    };
                    let multisample = vk::PipelineMultisampleStateCreateInfo {
                        rasterization_samples: vk::SampleCountFlags::TYPE_1,
                        ..Default::default()
                    };
                    let no_depth = vk::PipelineDepthStencilStateCreateInfo::default();
                    let blend_attach = [vk::PipelineColorBlendAttachmentState {
                        blend_enable: vk::FALSE,
                        color_write_mask: vk::ColorComponentFlags::RGBA,
                        ..Default::default()
                    }];
                    let blend = vk::PipelineColorBlendStateCreateInfo {
                        attachment_count: 1,
                        p_attachments: blend_attach.as_ptr(),
                        ..Default::default()
                    };
                    let dynamic = [vk::DynamicState::VIEWPORT, vk::DynamicState::SCISSOR];
                    let dyn_state = vk::PipelineDynamicStateCreateInfo {
                        dynamic_state_count: dynamic.len() as u32,
                        p_dynamic_states: dynamic.as_ptr(),
                        ..Default::default()
                    };
                    let pass_handle = pass.pass;
                    let info = vk::GraphicsPipelineCreateInfo {
                        stage_count: stages.len() as u32,
                        p_stages: stages.as_ptr(),
                        p_vertex_input_state: &vertex_input,
                        p_input_assembly_state: &asm,
                        p_viewport_state: &viewport_state,
                        p_rasterization_state: &raster,
                        p_multisample_state: &multisample,
                        p_depth_stencil_state: &no_depth,
                        p_color_blend_state: &blend,
                        p_dynamic_state: &dyn_state,
                        layout,
                        render_pass: pass_handle,
                        subpass: 0,
                        ..Default::default()
                    };
                    match device.device.create_graphics_pipelines(vk::PipelineCache::null(), &[info], None) {
                        Ok(mut pipes) => {
                            self.studio_pipe = pipes.remove(0);
                            self.studio_layout = Some(layout);
                            eprintln!("[studio] pipeline ready (vertex-color world + chat panel)");
                        }
                        Err((_, e)) => {
                            eprintln!("[studio] pipeline creation failed: {e:?} -- clear-only");
                        }
                    }
                    let _ = device.device.destroy_shader_module(vmod, None);
                    let _ = device.device.destroy_shader_module(fmod, None);
                }

                self.allocator = Some(allocator);

                self.entry = Some(entry);
                self.instance = Some(instance);
                self.surface = surface;
                self.physical = physical;
                self.device = Some(device);
                self.swapchain = Some(swapchain);
                self.render_pass = Some(pass);
                self.command_pool = Some(pool);
                self.cmd = Some(cmd);
                self.image_available = image_available;
                self.render_done = render_done;
                self.in_flight = in_flight;
                self.started = Instant::now();
            }

            // Framebuffers need &mut self helpers after fields settle
            self.create_framebuffers()?;
            eprintln!(
                "[vulkan] live on {} -- {}x{} swapchain, {} images",
                self.gpu_name,
                self.width,
                self.height,
                self.swapchain.as_ref().map(|s| s.images.len()).unwrap_or(0)
            );
            Ok(())
        }

        fn begin_frame(&mut self) -> Result<(), String> {
            let dev = self.device.as_ref().ok_or("no device")?;
            let sc = match self.swapchain.as_ref() {
                Some(s) => s,
                None => return Err("no swapchain".into()),
            };
            let _ = unsafe { dev.device.wait_for_fences(&[self.in_flight], true, u64::MAX) }
                ; // ignore timeout errors, treat as signaled
            let (index, _) = match acquire_next_image(
                &dev.device,
                &dev.swapchain_loader,
                sc.swapchain,
                u64::MAX,
                self.image_available,
                vk::Fence::null(),
            ) {
                Ok(v) => v,
                Err(vk::Result::ERROR_OUT_OF_DATE_KHR) => {
                    self.recreate_swapchain()?;
                    self.just_recreated = true;
                    return Ok(());
                }
                Err(e) => return Err(format!("acquire failed: {e:?}")),
            };
            self.frame_index = index;

            let cmd = self.cmd.unwrap();
            unsafe {
                dev.device.reset_fences(&[self.in_flight])
                    .map_err(|e| format!("reset fences: {e:?}"))?;
                dev.device.reset_command_buffer(cmd, vk::CommandBufferResetFlags::RELEASE_RESOURCES)
                    .map_err(|e| format!("reset cmd: {e:?}"))?;
                let begin = vk::CommandBufferBeginInfo::default();
                dev.device.begin_command_buffer(cmd, &begin)
                    .map_err(|e| format!("begin cmd: {e:?}"))?;
            }
            Ok(())
        }

        fn render(&mut self, scene: &litt_pathtracer::Scene, _camera: &litt_pathtracer::Camera) -> Result<(), String> {
            if self.just_recreated {
                return Ok(()); // swapchain recreated in begin_frame; skip this frame
            }
            let dev = self.device.as_ref().ok_or("no device")?;
            let sc = self.swapchain.as_ref().ok_or("no swapchain")?;
            let pass = self.render_pass.as_ref().ok_or("no render pass")?.pass;
            let idx = self.frame_index as usize;
            if idx >= self.framebuffers.len() {
                return Ok(());
            }
            let cmd = self.cmd.unwrap();

            // World-derived ambient tone: average material albedo, breathing
            // gently over time so the frame loop is visibly alive.
            let (r, g, b) = if !scene.materials.is_empty() {
                let mats = &scene.materials;
                let step = (mats.len() / 32).max(1);
                let (mut ar, mut ag, mut ab) = (0.0f32, 0.0f32, 0.0f32);
                let mut n = 0usize;
                for m in mats.iter().step_by(step) {
                    ar += m.albedo.0; ag += m.albedo.1; ab += m.albedo.2;
                    n += 1;
                }
                if n > 0 {
                    ((ar / n as f32).clamp(0.05, 0.9),
                     (ag / n as f32).clamp(0.05, 0.9),
                     (ab / n as f32).clamp(0.05, 0.9))
                } else { (0.10, 0.09, 0.14) }
            } else { (0.10, 0.09, 0.14) };
            let breathe = 0.5 + 0.5 * (self.started.elapsed().as_secs_f32().sin()) * 0.08;
            let clear = [r * breathe, g * breathe, b * breathe, 1.0];

            let subpass = [vk::ClearAttachment {
                aspect_mask: vk::ImageAspectFlags::COLOR,
                color_attachment: 0,
                clear_value: vk::ClearValue { color: vk::ClearColorValue { float32: clear } },
                ..Default::default()
            }];
            let sc_rect = vk::ClearRect {
                rect: vk::Rect2D {
                    offset: vk::Offset2D { x: 0, y: 0 },
                    extent: vk::Extent2D { width: sc.extents[0], height: sc.extents[1] },
                },
                base_array_layer: 0,
                layer_count: 1,
                ..Default::default()
            };

            unsafe {
                let rp_begin = vk::RenderPassBeginInfo {
                    render_pass: pass,
                    framebuffer: self.framebuffers[idx],
                    render_area: sc_rect.rect,
                    clear_value_count: 1,
                    p_clear_values: [vk::ClearValue {
                        color: vk::ClearColorValue { float32: clear },
                    }].as_ptr(),
                    ..Default::default()
                };
                dev.device.cmd_begin_render_pass(cmd, &rp_begin, vk::SubpassContents::INLINE);
                dev.device.cmd_clear_attachments(cmd, &subpass, &[sc_rect]);

                // ---- Studio draws: world viewport (right), chat panel (left)
                if self.studio_pipe == vk::Pipeline::null() {
                    let layout = match self.studio_layout {
                        Some(l) => l,
                        None => {
                            dev.device.cmd_end_render_pass(cmd);
                            return Ok(());
                        }
                    };
                    let full_w = sc.extents[0];
                    let full_h = sc.extents[1];
                    let panel = STUDIO_PANEL_W.min(full_w.saturating_sub(1));

                    if self.world_count > 0 {
                        if let Some((buf, _, _)) = self.world_buf.as_ref() {
                            let vx = panel;
                            let vw = full_w - vx;
                            dev.device.cmd_bind_pipeline(
                                cmd, vk::PipelineBindPoint::GRAPHICS, self.studio_pipe);
                            dev.device.cmd_set_viewport(cmd, 0, &[vk::Viewport {
                                x: vx as f32, y: 0.0,
                                width: vw as f32, height: full_h as f32,
                                min_depth: 0.0, max_depth: 1.0,
                            }]);
                            dev.device.cmd_set_scissor(cmd, 0, &[vk::Rect2D {
                                offset: vk::Offset2D { x: vx as i32, y: 0 },
                                extent: vk::Extent2D { width: vw, height: full_h },
                            }]);
                            dev.device.cmd_bind_vertex_buffers(cmd, 0, &[*buf], &[0]);
                            dev.device.cmd_push_constants(
                                cmd, layout, vk::ShaderStageFlags::VERTEX, 0,
                                std::slice::from_raw_parts(
                                    self.world_mvp.as_ptr() as *const u8, 64),
                            );
                            dev.device.cmd_draw(cmd, self.world_count, 1, 0, 0);
                        }
                    }

                    if self.ui_count > 0 && panel > 0 {
                        if let Some((buf, _, _)) = self.ui_buf.as_ref() {
                            dev.device.cmd_bind_pipeline(
                                cmd, vk::PipelineBindPoint::GRAPHICS, self.studio_pipe);
                            dev.device.cmd_set_viewport(cmd, 0, &[vk::Viewport {
                                x: 0.0, y: 0.0,
                                width: panel as f32, height: full_h as f32,
                                min_depth: 0.0, max_depth: 1.0,
                            }]);
                            dev.device.cmd_set_scissor(cmd, 0, &[vk::Rect2D {
                                offset: vk::Offset2D { x: 0, y: 0 },
                                extent: vk::Extent2D { width: panel, height: full_h },
                            }]);
                            dev.device.cmd_bind_vertex_buffers(cmd, 0, &[*buf], &[0]);
                            // pixel-space ortho; y down
                            let mvp = panel_ortho(panel, full_h);
                            let bytes = std::slice::from_raw_parts(
                                mvp.as_ptr() as *const u8, 64);
                            dev.device.cmd_push_constants(
                                cmd, layout, vk::ShaderStageFlags::VERTEX, 0, bytes,
                            );
                            dev.device.cmd_draw(cmd, self.ui_count, 1, 0, 0);
                        }
                    }
                }

                dev.device.cmd_end_render_pass(cmd);
            }
            Ok(())
        }

        fn present(&mut self) -> Result<(), String> {
            if self.just_recreated {
                self.just_recreated = false;
                return Ok(());
            }
            let dev = self.device.as_ref().ok_or("no device")?;
            let sc = match self.swapchain.as_ref() {
                Some(s) => s,
                None => return Ok(()),
            };
            let cmd = self.cmd.unwrap();
            unsafe {
                dev.device.end_command_buffer(cmd)
                    .map_err(|e| format!("end cmd: {e:?}"))?;
                let wait_stage = [vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT];
                let submit = vk::SubmitInfo {
                    wait_semaphore_count: 1,
                    p_wait_semaphores: &self.image_available,
                    p_wait_dst_stage_mask: wait_stage.as_ptr(),
                    signal_semaphore_count: 1,
                    p_signal_semaphores: &self.render_done,
                    command_buffer_count: 1,
                    p_command_buffers: &cmd,
                    ..Default::default()
                };
                dev.device.queue_submit(dev.draw_queue, &[submit], self.in_flight)
                    .map_err(|e| format!("submit: {e:?}"))?;
            }
            match vk_present(&dev.swapchain_loader, sc.swapchain, dev.draw_queue,
                             self.frame_index, self.render_done) {
                Ok(_) | Err(()) => Ok(()),
            }
        }

        fn end_frame(&mut self) -> Result<(), String> { Ok(()) }

        fn studio_ready(&self) -> bool {
            !(self.studio_pipe == vk::Pipeline::null())
        }

        fn set_world_mvp(&mut self, mvp: [f32; 16]) {
            self.world_mvp = mvp;
        }

        fn upload_world_mesh(&mut self, verts: &[f32]) {
            let bytes_len = (verts.len() * 4) as u64;
            if verts.is_empty() {
                self.world_count = 0;
                return;
            }
            if let (Some(dev), Some(alloc)) = (self.device.as_ref(), self.allocator.as_mut()) {
                match ensure_host_buffer(dev, alloc, &mut self.world_buf, bytes_len) {
                    Ok(()) => {
                        write_host_buffer(&mut self.world_buf, verts);
                        self.world_count = (verts.len() / 6) as u32;
                    }
                    Err(e) => eprintln!("[studio] world buffer: {e}"),
                }
            }
        }

        fn upload_panel_mesh(&mut self, verts: &[f32]) {
            let bytes_len = (verts.len() * 4) as u64;
            if verts.is_empty() {
                self.ui_count = 0;
                return;
            }
            if let (Some(dev), Some(alloc)) = (self.device.as_ref(), self.allocator.as_mut()) {
                match ensure_host_buffer(dev, alloc, &mut self.ui_buf, bytes_len) {
                    Ok(()) => {
                        write_host_buffer(&mut self.ui_buf, verts);
                        self.ui_count = (verts.len() / 6) as u32;
                    }
                    Err(e) => eprintln!("[studio] panel buffer: {e}"),
                }
            }
        }

        fn shutdown(&mut self) -> Result<(), String> {
            if let Some(dev) = self.device.take() {
                unsafe {
                    let _ = dev.device.device_wait_idle();
                    // Studio resources first
                    if self.studio_pipe == vk::Pipeline::null() {
                        dev.device.destroy_pipeline(self.studio_pipe, None);
                        self.studio_pipe = vk::Pipeline::null();
                    }
                    if let Some(layout) = self.studio_layout.take() {
                        dev.device.destroy_pipeline_layout(layout, None);
                    }
                    if let Some(alloc) = self.allocator.as_mut() {
                        if let Some((buf, mut a, _)) = self.world_buf.take() {
                            alloc.free_buffer(buf, &mut a);
                        }
                        if let Some((buf, mut a, _)) = self.ui_buf.take() {
                            alloc.free_buffer(buf, &mut a);
                        }
                    }
                    for fb in self.framebuffers.drain(..) {
                        dev.device.destroy_framebuffer(fb, None);
                    }
                    if let Some(sc) = self.swapchain.take() {
                        destroy_swapchain(&dev.device, &dev.swapchain_loader, sc.swapchain, &sc.views);
                    }
                    dev.device.destroy_semaphore(self.image_available, None);
                    dev.device.destroy_semaphore(self.render_done, None);
                    dev.device.destroy_fence(self.in_flight, None);
                }
                drop(self.command_pool.take());   // destroys pool
                drop(self.render_pass.take());    // destroys pass
                if self.surface != vk::SurfaceKHR::null() {
                    unsafe { dev.surface_loader.destroy_surface(self.surface, None) };
                }
                // device Drop waits idle + destroys; instance dropped last
                drop(dev);
                drop(self.instance.take());
            }
            Ok(())
        }
    }
}

/// DX12 backend wrapper
#[cfg(feature = "dx12")]
pub mod dx12 {
    use super::*;
    use litt_dx12::*;

    pub struct Dx12Backend {
        pub device: Option<litt_dx12::D3D12Device>,
        pub features: GraphicsFeatures,
    }

    impl Dx12Backend {
        pub fn new() -> Self {
            Self {
                device: None,
                features: GraphicsFeatures::default(),
            }
        }

        pub fn init(&mut self) -> Result<(), String> {
            // DX12 initialization
            Ok(())
        }
    }

    impl GraphicsBackend for Dx12Backend {
        fn name(&self) -> &str { "DX12" }

        fn supports_ray_tracing(&self) -> bool {
            self.features.ray_tracing
        }

        fn supports_mesh_shaders(&self) -> bool {
            true
        }

        fn adapter_info(&self) -> &str {
            "DX12 (Windows native)"
        }

        fn initialize(&mut self, _width: u32, _height: u32) -> Result<(), String> {
            self.init()
        }

        fn begin_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn render(&mut self, _scene: &litt_pathtracer::Scene, _camera: &litt_pathtracer::Camera) -> Result<(), String> { Ok(()) }
        fn present(&mut self) -> Result<(), String> { Ok(()) }
        fn end_frame(&mut self) -> Result<(), String> { Ok(()) }
        fn shutdown(&mut self) -> Result<(), String> { Ok(()) }
    }
}

/// Select the best graphics backend (uninitialized -- call set_window +
/// initialize on the returned backend before first use).
pub fn select_backend() -> Result<Box<dyn GraphicsBackend>, String> {
    #[cfg(feature = "vulkan")]
    {
        return Ok(Box::new(vulkan::VulkanBackend::new()));
    }

    #[cfg(all(not(feature = "vulkan"), feature = "dx12"))]
    {
        return Ok(Box::new(dx12::Dx12Backend::new()));
    }

    #[allow(unreachable_code)]
    Err("No graphics backend available".to_string())
}

/// Get the detected GPU info
pub fn get_gpu_info() -> String {
    if cfg!(feature = "dx12") {
        return "DX12 (Windows native)".to_string();
    }
    if cfg!(feature = "vulkan") {
        return "Vulkan".to_string();
    }
    "Unknown".to_string()
}

    /// Column-major ortho mapping pixel coords (y down) to NDC.
    fn panel_ortho(w: u32, h: u32) -> [f32; 16] {
        let wf = w as f32;
        let hf = h as f32;
        [
            2.0 / wf, 0.0, 0.0, 0.0,
            0.0, -2.0 / hf, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            -1.0, 1.0, 0.0, 1.0,
        ]
    }
