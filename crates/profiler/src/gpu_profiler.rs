//! GPU profiler -- measures GPU execution time using Vulkan timestamps.

use ash::vk::Handle;
use ash::{vk, Device};

/// GPU timer query (ash::Device is not Debug, so no derive here)
pub struct GpuTimerQuery {
    pub query_pool: vk::QueryPool,
    pub device: Device,
    pub next_query: u32,
    pub query_count: u32,
}

impl std::fmt::Debug for GpuTimerQuery {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GpuTimerQuery")
            .field("query_pool", &self.query_pool.as_raw())
            .field("next_query", &self.next_query)
            .field("query_count", &self.query_count)
            .finish()
    }
}

impl GpuTimerQuery {
    pub fn new(device: &Device, query_count: u32) -> Result<Self, String> {
        let info = vk::QueryPoolCreateInfo {
            query_type: vk::QueryType::TIMESTAMP,
            query_count: query_count * 2, // begin + end pairs
            ..Default::default()
        };
        let query_pool = unsafe {
            device
                .create_query_pool(&info, None)
                .map_err(|e| format!("Failed to create query pool: {:?}", e))?
        };
        Ok(Self {
            query_pool,
            device: device.clone(),
            next_query: 0,
            query_count,
        })
    }

    /// Begin a timed region
    pub unsafe fn begin(&self, command_buffer: vk::CommandBuffer, query_idx: u32) {
        self.device.cmd_write_timestamp(
            command_buffer,
            vk::PipelineStageFlags::TOP_OF_PIPE,
            self.query_pool,
            query_idx * 2,
        );
    }

    /// End a timed region
    pub unsafe fn end(&self, command_buffer: vk::CommandBuffer, query_idx: u32) {
        self.device.cmd_write_timestamp(
            command_buffer,
            vk::PipelineStageFlags::BOTTOM_OF_PIPE,
            self.query_pool,
            query_idx * 2 + 1,
        );
    }
}

impl Drop for GpuTimerQuery {
    fn drop(&mut self) {
        unsafe {
            self.device.destroy_query_pool(self.query_pool, None);
        }
    }
}

/// GPU profiler data
#[derive(Debug, Default)]
pub struct GpuProfileData {
    pub draw_call_time_ms: f32,
    pub compute_time_ms: f32,
    pub transfer_time_ms: f32,
    pub total_gpu_time_ms: f32,
}
