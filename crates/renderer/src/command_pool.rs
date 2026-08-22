//! Command pool management with single-shot command buffer helper.

use ash::{vk, Device};

#[derive(Debug)]
pub struct CommandPool {
    pub pool: vk::CommandPool,
}

impl CommandPool {
    pub fn new(device: &Device, queue_family: u32) -> Result<Self, String> {
        let info = vk::CommandPoolCreateInfo::builder()
            .queue_family_index(queue_family)
            .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER)
            .build();
        let pool = unsafe { device.create_command_pool(&info, None)
            .map_err(|e| format!("Command pool creation failed: {:?}", e))? };
        Ok(Self { pool })
    }

    /// Begin, record, and submit a single-shot command buffer, then end it.
    /// Returns the recorded command buffer handle.
    pub fn begin_single_time_commands(&self, device: &Device) -> Result<vk::CommandBuffer, String> {
        let alloc_info = vk::CommandBufferAllocateInfo::builder()
            .command_pool(self.pool)
            .level(vk::CommandBufferLevel::PRIMARY)
            .command_buffer_count(1)
            .build();
        let mut cmd = unsafe {
            device.allocate_command_buffers(&alloc_info)
                .map_err(|e| format!("Command buffer alloc failed: {:?}", e))?
        };
        let cmd = cmd[0];

        let begin = vk::CommandBufferBeginInfo::builder()
            .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT)
            .build();
        unsafe {
            device.begin_command_buffer(cmd, &begin)
                .map_err(|e| format!("Begin command buffer failed: {:?}", e))?;
        }
        Ok(cmd)
    }

    /// End the command buffer and submit it to the queue, then wait for completion.
    pub fn end_single_time_commands(
        &self,
        cmd: vk::CommandBuffer,
        device: &Device,
        queue: vk::Queue,
    ) -> Result<(), String> {
        unsafe {
            device.end_command_buffer(cmd)
                .map_err(|e| format!("End command buffer failed: {:?}", e))?;

            let submit = vk::SubmitInfo::builder()
                .command_buffers(&[cmd])
                .build();
            device.queue_submit(queue, &[submit], vk::Fence::null())
                .map_err(|e| format!("Queue submit failed: {:?}", e))?;
            device.queue_wait_idle(queue)
                .map_err(|e| format!("Queue wait idle failed: {:?}", e))?;
        }
        Ok(())
    }

    /// Issue an image layout transition barrier.
    ///
    /// `src_layout` / `dst_layout` -- the layouts to transition between.
    /// `image` -- the image to transition.
    /// `aspect_mask` -- typically `vk::ImageAspectFlags::COLOR`.
    /// `src_stage` / `dst_stage` -- pipeline stages for the barrier.
    /// `src_access` / `dst_access` -- access flags for the barrier.
    pub fn transition_image_layout(
        &self,
        cmd: vk::CommandBuffer,
        device: &Device,
        image: vk::Image,
        aspect_mask: vk::ImageAspectFlags,
        src_layout: vk::ImageLayout,
        dst_layout: vk::ImageLayout,
        src_stage: vk::PipelineStageFlags,
        dst_stage: vk::PipelineStageFlags,
        src_access: vk::AccessFlags,
        dst_access: vk::AccessFlags,
    ) -> Result<(), String> {
        let barrier = vk::ImageMemoryBarrier {
            s_type: vk::StructureType::IMAGE_MEMORY_BARRIER,
            src_access_mask: src_access,
            dst_access_mask: dst_access,
            old_layout: src_layout,
            new_layout: dst_layout,
            src_queue_family_index: vk::QUEUE_FAMILY_IGNORED,
            dst_queue_family_index: vk::QUEUE_FAMILY_IGNORED,
            image,
            subresource_range: vk::ImageSubresourceRange {
                aspect_mask,
                base_mip_level: 0,
                level_count: 1,
                base_array_layer: 0,
                layer_count: 1,
            },
            ..Default::default()
        };
        unsafe {
            device.cmd_pipeline_barrier(
                cmd,
                src_stage,
                dst_stage,
                vk::DependencyFlags::empty(),
                &[],
                &[],
                &[barrier],
            );
        }
        Ok(())
    }
}

impl Drop for CommandPool {
    fn drop(&mut self) {
        unsafe { self.device.destroy_command_pool(self.pool, None); }
    }
}
