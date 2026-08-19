//! Pipeline creation for graphics and compute operations.

use ash::{vk, Device};
use super::*;

/// Pipeline cache for AMD optimization
pub struct PipelineCache {
    cache: vk::PipelineCache,
    device: Device,
}

impl PipelineCache {
    pub fn new(device: &Device) -> Result<Self, String> {
        let info = vk::PipelineCacheCreateInfo::builder()
            .initial_data(&[])
            .build();
        let cache = unsafe { device.create_pipeline_cache(&info, None)
            .map_err(|e| format!("Failed to create pipeline cache: {:?}", e))? };
        Ok(Self { cache, device: device.clone() })
    }

    pub fn inner(&self) -> vk::PipelineCache { self.cache }
}

impl Drop for PipelineCache {
    fn drop(&mut self) {
        unsafe { self.device.destroy_pipeline_cache(self.cache, None); }
    }
}

/// Create a compute pipeline from SPIR-V
pub fn create_compute_pipeline(
    device: &Device,
    shader_data: &[u32],
    push_constant_size: u32,
    descriptor_set_layouts: &[vk::DescriptorSetLayout],
) -> Result<ComputePipeline, String> {
    // Create shader module
    let shader_info = vk::ShaderModuleCreateInfo::builder()
        .code(shader_data)
        .build();
    let shader_module = unsafe { device.create_shader_module(&shader_info, None)
        .map_err(|e| format!("Failed to create shader module: {:?}", e))? };

    // Pipeline layout
    let layout_info = vk::PipelineLayoutCreateInfo::builder()
        .push_constant_ranges(&[vk::PushConstantRange {
            stage_flags: vk::ShaderStageFlags::COMPUTE,
            offset: 0,
            size: push_constant_size,
        }])
        .set_layouts(descriptor_set_layouts)
        .build();
    let layout = unsafe { device.create_pipeline_layout(&layout_info, None)
        .map_err(|e| format!("Failed to create pipeline layout: {:?}", e))? };

    // Compute pipeline
    let pipeline_info = vk::ComputePipelineCreateInfo::builder()
        .stage(vk::PipelineShaderStageCreateInfo {
            s_type: vk::StructureType::PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage: vk::ShaderStageFlags::COMPUTE,
            module: shader_module,
            p_name: std::ffi::CString::new("main").unwrap().as_ptr(),
            ..Default::default()
        })
        .layout(layout)
        .build();

    let pipeline = unsafe { device.create_compute_pipeline(&pipeline_info, None)
        .map_err(|e| format!("Failed to create compute pipeline: {:?}", e))? };

    unsafe { device.destroy_shader_module(shader_module, None); }

    Ok(ComputePipeline { pipeline, layout })
}

/// Create a graphics pipeline (minimal quad for debug)
pub fn create_graphics_pipeline(
    device: &Device,
    render_pass: vk::RenderPass,
    shader_data: &[u32],
    descriptor_set_layouts: &[vk::DescriptorSetLayout],
    viewport: vk::Viewport,
    scissor: vk::Rect2D,
) -> Result<GraphicsPipeline, String> {
    // Shader module
    let shader_info = vk::ShaderModuleCreateInfo::builder()
        .code(shader_data)
        .build();
    let shader_module = unsafe { device.create_shader_module(&shader_info, None)
        .map_err(|e| format!("Failed to create shader module: {:?}", e))? };

    // Pipeline layout
    let layout_info = vk::PipelineLayoutCreateInfo::builder()
        .set_layouts(descriptor_set_layouts)
        .build();
    let layout = unsafe { device.create_pipeline_layout(&layout_info, None)
        .map_err(|e| format!("Failed to create pipeline layout: {:?}", e))? };

    // Vertex input
    let binding_desc = vk::VertexInputBindingDescription {
        binding: 0,
        stride: std::mem::size_of::<[f32; 3]>() as u32,
        input_rate: vk::VertexInputRate::VERTEX,
    };
    let attribute_desc = vk::VertexInputAttributeDescription {
        binding: 0,
        location: 0,
        format: vk::Format::R32G32B32_SFLOAT,
        offset: 0,
    };

    // Input assembly
    let ia = vk::InputAssemblyStateCreateInfo::builder()
        .topology(vk::PrimitiveTopology::TRIANGLE_LIST)
        .build();

    // Viewport and scissor
    let vp = vk::ViewportStateCreateInfo::builder()
        .viewports(&[viewport])
        .scissors(&[scissor])
        .build();

    // Rasterization
    let raster = vk::RasterizationStateCreateInfo::builder()
        .polygon_mode(vk::PolygonMode::FILL)
        .cull_mode(vk::CullModeFlags::NONE)
        .front_face(vk::FrontFace::COUNTER_CLOCKWISE)
        .build();

    // Color blend
    let blend = vk::PipelineColorBlendStateCreateInfo::builder()
        .logic_op(vk::LogicOp::COPY)
        .build();

    // Pipeline
    let info = vk::GraphicsPipelineCreateInfo::builder()
        .stage(vk::PipelineShaderStageCreateInfo {
            s_type: vk::StructureType::PIPELINE_SHADER_STAGE_CREATE_INFO,
            stage: vk::ShaderStageFlags::VERTEX | vk::ShaderStageFlags::FRAGMENT,
            module: shader_module,
            p_name: std::ffi::CString::new("main").unwrap().as_ptr(),
            ..Default::default()
        })
        .vertex_input_state(&vk::PipelineVertexInputStateCreateInfo::builder()
            .vertex_binding_descriptions(&[binding_desc])
            .vertex_attribute_descriptions(&[attribute_desc])
            .build())
        .input_assembly_state(&ia)
        .viewport_state(&vp)
        .rasterization_state(&raster)
        .color_blend_state(&blend)
        .render_pass(render_pass)
        .layout(layout)
        .build();

    let pipeline = unsafe { device.create_graphics_pipelines(
        vk::PipelineCache::null(), &[info], None
    ).map_err(|e| format!("Failed to create graphics pipeline: {:?}", e))?[0] };

    unsafe { device.destroy_shader_module(shader_module, None); }

    Ok(GraphicsPipeline { pipeline, layout })
}
