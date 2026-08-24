//! Pipeline creation for graphics and compute operations (ash 0.38).

use ash::vk;
use ash::Device;

/// Owned compute pipeline + layout.
pub struct ComputePipeline {
    pub pipeline: vk::Pipeline,
    pub layout: vk::PipelineLayout,
}

/// Owned graphics pipeline + layout.
pub struct GraphicsPipeline {
    pub pipeline: vk::Pipeline,
    pub layout: vk::PipelineLayout,
}

/// Pipeline cache wrapper
pub struct PipelineCache {
    cache: vk::PipelineCache,
    device: Device,
}

impl PipelineCache {
    pub fn new(device: &Device) -> Result<Self, String> {
        let info = vk::PipelineCacheCreateInfo {
            initial_data_size: 0,
            p_initial_data: std::ptr::null(),
            ..Default::default()
        };
        let cache = unsafe {
            device
                .create_pipeline_cache(&info, None)
                .map_err(|e| format!("Failed to create pipeline cache: {e:?}"))?
        };
        Ok(Self { cache, device: device.clone() })
    }

    pub fn inner(&self) -> vk::PipelineCache {
        self.cache
    }
}

impl Drop for PipelineCache {
    fn drop(&mut self) {
        unsafe { self.device.destroy_pipeline_cache(self.cache, None) };
    }
}

fn make_stage(
    stage: vk::ShaderStageFlags,
    module: vk::ShaderModule,
    name: &std::ffi::CStr,
) -> vk::PipelineShaderStageCreateInfo<'static> {
    vk::PipelineShaderStageCreateInfo {
        stage,
        module,
        p_name: name.as_ptr(),
        ..Default::default()
    }
}

/// Create a compute pipeline from SPIR-V
pub fn create_compute_pipeline(
    device: &Device,
    shader_data: &[u32],
    push_constant_size: u32,
    descriptor_set_layouts: &[vk::DescriptorSetLayout],
) -> Result<ComputePipeline, String> {
    let main = std::ffi::CString::new("main").unwrap();

    // Shader module
    let shader_info = vk::ShaderModuleCreateInfo {
        code_size: shader_data.len() * 4,
        p_code: shader_data.as_ptr(),
        ..Default::default()
    };
    let shader_module = unsafe {
        device
            .create_shader_module(&shader_info, None)
            .map_err(|e| format!("Failed to create shader module: {e:?}"))?
    };

    // Pipeline layout
    let push_constants = [vk::PushConstantRange {
        stage_flags: vk::ShaderStageFlags::COMPUTE,
        offset: 0,
        size: push_constant_size,
    }];
    let layout_info = vk::PipelineLayoutCreateInfo {
        set_layout_count: descriptor_set_layouts.len() as u32,
        p_set_layouts: descriptor_set_layouts.as_ptr(),
        push_constant_range_count: push_constants.len() as u32,
        p_push_constant_ranges: push_constants.as_ptr(),
        ..Default::default()
    };
    let layout = unsafe {
        device
            .create_pipeline_layout(&layout_info, None)
            .map_err(|e| format!("Failed to create pipeline layout: {e:?}"))?
    };

    // Compute pipeline
    let stage = make_stage(vk::ShaderStageFlags::COMPUTE, shader_module, &main);
    let pipeline_info = vk::ComputePipelineCreateInfo {
        stage,
        layout,
        ..Default::default()
    };

    let result = unsafe {
        device.create_compute_pipelines(vk::PipelineCache::null(), &[pipeline_info], None)
    };
    unsafe { device.destroy_shader_module(shader_module, None) };
    let pipelines =
        result.map_err(|(_, e)| format!("Failed to create compute pipeline: {e:?}"))?;

    Ok(ComputePipeline { pipeline: pipelines[0], layout })
}

/// Create a graphics pipeline (minimal triangle for debug).
#[allow(clippy::too_many_arguments)]
pub fn create_graphics_pipeline(
    device: &Device,
    render_pass: vk::RenderPass,
    shader_data: &[u32],
    descriptor_set_layouts: &[vk::DescriptorSetLayout],
    viewport: vk::Viewport,
    scissor: vk::Rect2D,
) -> Result<GraphicsPipeline, String> {
    let main = std::ffi::CString::new("main").unwrap();

    // Shader module
    let shader_info = vk::ShaderModuleCreateInfo {
        code_size: shader_data.len() * 4,
        p_code: shader_data.as_ptr(),
        ..Default::default()
    };
    let shader_module = unsafe {
        device
            .create_shader_module(&shader_info, None)
            .map_err(|e| format!("Failed to create shader module: {e:?}"))?
    };

    // Pipeline layout
    let layout_info = vk::PipelineLayoutCreateInfo {
        set_layout_count: descriptor_set_layouts.len() as u32,
        p_set_layouts: descriptor_set_layouts.as_ptr(),
        ..Default::default()
    };
    let layout = unsafe {
        device
            .create_pipeline_layout(&layout_info, None)
            .map_err(|e| format!("Failed to create pipeline layout: {e:?}"))?
    };

    // Vertex input
    let binding_desc = [vk::VertexInputBindingDescription {
        binding: 0,
        stride: std::mem::size_of::<[f32; 3]>() as u32,
        input_rate: vk::VertexInputRate::VERTEX,
    }];
    let attribute_desc = [vk::VertexInputAttributeDescription {
        binding: 0,
        location: 0,
        format: vk::Format::R32G32B32_SFLOAT,
        offset: 0,
    }];

    // States
    let vertex_input = vk::PipelineVertexInputStateCreateInfo {
        vertex_binding_description_count: binding_desc.len() as u32,
        p_vertex_binding_descriptions: binding_desc.as_ptr(),
        vertex_attribute_description_count: attribute_desc.len() as u32,
        p_vertex_attribute_descriptions: attribute_desc.as_ptr(),
        ..Default::default()
    };
    let ia = vk::PipelineInputAssemblyStateCreateInfo {
        topology: vk::PrimitiveTopology::TRIANGLE_LIST,
        ..Default::default()
    };
    let viewports = [viewport];
    let scissors = [scissor];
    let vp = vk::PipelineViewportStateCreateInfo {
        viewport_count: viewports.len() as u32,
        p_viewports: viewports.as_ptr(),
        scissor_count: scissors.len() as u32,
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
    let blend_attachment = [vk::PipelineColorBlendAttachmentState {
        color_write_mask: vk::ColorComponentFlags::RGBA,
        blend_enable: ash::vk::FALSE,
        ..Default::default()
    }];
    let blend = vk::PipelineColorBlendStateCreateInfo {
        logic_op: vk::LogicOp::COPY,
        attachment_count: blend_attachment.len() as u32,
        p_attachments: blend_attachment.as_ptr(),
        ..Default::default()
    };

    // Stages: vertex + fragment from one module
    let stages = [
        make_stage(vk::ShaderStageFlags::VERTEX, shader_module, &main),
        make_stage(vk::ShaderStageFlags::FRAGMENT, shader_module, &main),
    ];

    let info = vk::GraphicsPipelineCreateInfo {
        stage_count: stages.len() as u32,
        p_stages: stages.as_ptr(),
        p_vertex_input_state: &vertex_input,
        p_input_assembly_state: &ia,
        p_viewport_state: &vp,
        p_rasterization_state: &raster,
        p_multisample_state: &multisample,
        p_color_blend_state: &blend,
        render_pass,
        subpass: 0,
        layout,
        ..Default::default()
    };

    let result = unsafe {
        device.create_graphics_pipelines(vk::PipelineCache::null(), &[info], None)
    };
    unsafe { device.destroy_shader_module(shader_module, None) };
    let pipelines =
        result.map_err(|(_, e)| format!("Failed to create graphics pipeline: {e:?}"))?;

    Ok(GraphicsPipeline { pipeline: pipelines[0], layout })
}
