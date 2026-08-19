//! Render pass for path tracing (compute-only) and display (graphics).

use ash::{vk, Device};

#[derive(Debug)]
pub struct RenderPass {
    pub pass: vk::RenderPass,
}

impl RenderPass {
    pub fn new(device: &Device, swapchain_format: vk::Format) -> Result<Self, String> {
        let attachment = vk::AttachmentDescription {
            format: swapchain_format,
            samples: vk::SampleCountFlags::TYPE_1,
            load_op: vk::AttachmentLoadOp::CLEAR,
            store_op: vk::AttachmentStoreOp::STORE,
            stencil_load_op: vk::AttachmentLoadOp::DONT_CARE,
            stencil_store_op: vk::AttachmentStoreOp::DONT_CARE,
            initial_layout: vk::ImageLayout::UNDEFINED,
            final_layout: vk::ImageLayout::PRESENT_SRC_KHR,
        };

        let attachment_ref = vk::AttachmentReference {
            attachment: 0,
            layout: vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        };

        let subpass = vk::SubpassDescription {
            pipeline_bind_point: vk::PipelineBindPoint::GRAPHICS,
            color_attachments: &[Some(attachment_ref)],
            ..Default::default()
        };

        let dependency = vk::SubpassDependency {
            src_subpass: vk::SUBPASS_EXTERNAL,
            dst_subpass: 0,
            src_stage_mask: vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT,
            dst_stage_mask: vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT,
            src_access_mask: vk::AccessFlags::empty(),
            dst_access_mask: vk::AccessFlags::COLOR_ATTACHMENT_WRITE,
            dependency_flags: vk::DependencyFlags::NONE,
        };

        let info = vk::RenderPassCreateInfo::builder()
            .attachments(&[attachment])
            .subpasses(&[subpass])
            .dependencies(&[dependency])
            .build();

        let pass = unsafe { device.create_render_pass(&info, None)
            .map_err(|e| format!("Render pass creation failed: {:?}", e))? };

        Ok(Self { pass })
    }
}

impl Drop for RenderPass {
    fn drop(&mut self) {
        unsafe { self.device.destroy_render_pass(self.pass, None); }
    }
}
