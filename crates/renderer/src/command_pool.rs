//! Command pool management.

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
}

impl Drop for CommandPool {
    fn drop(&mut self) {
        unsafe { self.device.destroy_command_pool(self.pool, None); }
    }
}
