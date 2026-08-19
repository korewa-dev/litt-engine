//! Descriptor pool and set management.

use ash::{vk, Device};

#[derive(Debug)]
pub struct DescriptorPool {
    pub pool: vk::DescriptorPool,
    max_sets: u32,
}

impl DescriptorPool {
    pub fn new(device: &Device, max_sets: u32) -> Result<Self, String> {
        let pool_sizes = &[vk::DescriptorPoolSize {
            type_: vk::DescriptorType::UNIFORM_BUFFER,
            descriptor_count: max_sets * 4,
        }, vk::DescriptorPoolSize {
            type_: vk::DescriptorType::COMBINED_IMAGE_SAMPLER,
            descriptor_count: max_sets * 4,
        }, vk::DescriptorPoolSize {
            type_: vk::DescriptorType::STORAGE_BUFFER,
            descriptor_count: max_sets * 2,
        }, vk::DescriptorPoolSize {
            type_: vk::DescriptorType::ACCELERATION_STRUCTURE_KHR,
            descriptor_count: max_sets,
        }];

        let info = vk::DescriptorPoolCreateInfo::builder()
            .max_sets(max_sets)
            .pool_sizes(pool_sizes)
            .build();

        let pool = unsafe { device.create_descriptor_pool(&info, None)
            .map_err(|e| format!("Descriptor pool creation failed: {:?}", e))? };

        Ok(Self { pool, max_sets })
    }

    pub fn allocate(&self, device: &Device, layout: &[vk::DescriptorSetLayout]) -> Result<vk::DescriptorSet, String> {
        let info = vk::DescriptorSetAllocateInfo::builder()
            .descriptor_pool(self.pool)
            .set_layouts(layout)
            .build();
        unsafe {
            device.allocate_descriptor_sets(&info)
                .map_err(|e| format!("Descriptor allocation failed: {:?}", e))
                .map(|v| v[0])
        }
    }

    pub fn reset(&self, device: &Device) -> Result<(), String> {
        unsafe { device.reset_descriptor_pool(self.pool, vk::DescriptorPoolCreateFlags::RESET_COMMAND_BUFFER)
            .map_err(|e| format!("Descriptor pool reset failed: {:?}", e)) }
    }
}

impl Drop for DescriptorPool {
    fn drop(&mut self) {
        unsafe { self.device.destroy_descriptor_pool(self.pool, None); }
    }
}
