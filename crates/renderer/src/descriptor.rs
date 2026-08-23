//! Descriptor pool and set management.

use ash::{vk, Device};

pub struct DescriptorPool {
    pub pool: vk::DescriptorPool,
    pub device: Device,
    max_sets: u32,
}

impl DescriptorPool {
    pub fn new(device: &Device, max_sets: u32) -> Result<Self, String> {
        let pool_sizes = [
            vk::DescriptorPoolSize {
                ty: vk::DescriptorType::UNIFORM_BUFFER,
                descriptor_count: max_sets * 4,
            },
            vk::DescriptorPoolSize {
                ty: vk::DescriptorType::COMBINED_IMAGE_SAMPLER,
                descriptor_count: max_sets * 4,
            },
            vk::DescriptorPoolSize {
                ty: vk::DescriptorType::STORAGE_BUFFER,
                descriptor_count: max_sets * 2,
            },
            vk::DescriptorPoolSize {
                ty: vk::DescriptorType::STORAGE_IMAGE,
                descriptor_count: max_sets * 2,
            },
            vk::DescriptorPoolSize {
                ty: vk::DescriptorType::ACCELERATION_STRUCTURE_KHR,
                descriptor_count: max_sets,
            },
        ];

        let info = vk::DescriptorPoolCreateInfo {
            max_sets,
            pool_size_count: pool_sizes.len() as u32,
            p_pool_sizes: pool_sizes.as_ptr(),
            ..Default::default()
        };

        let pool = unsafe { device.create_descriptor_pool(&info, None)
            .map_err(|e| format!("Descriptor pool creation failed: {:?}", e))? };

        Ok(Self { pool, device: device.clone(), max_sets })
    }

    pub fn allocate(&self, _device: &Device, layout: &[vk::DescriptorSetLayout]) -> Result<vk::DescriptorSet, String> {
        let info = vk::DescriptorSetAllocateInfo {
            descriptor_pool: self.pool,
            descriptor_set_count: layout.len() as u32,
            p_set_layouts: layout.as_ptr(),
            ..Default::default()
        };
        unsafe {
            self.device.allocate_descriptor_sets(&info)
                .map_err(|e| format!("Descriptor allocation failed: {:?}", e))
                .map(|v| v[0])
        }
    }

    pub fn reset(&self) -> Result<(), String> {
        unsafe { self.device.reset_descriptor_pool(self.pool, vk::DescriptorPoolResetFlags::empty())
            .map_err(|e| format!("Descriptor pool reset failed: {:?}", e)) }
    }

    pub fn capacity(&self) -> u32 {
        self.max_sets
    }
}

impl Drop for DescriptorPool {
    fn drop(&mut self) {
        unsafe { self.device.destroy_descriptor_pool(self.pool, None); }
    }
}



impl std::fmt::Debug for DescriptorPool {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DescriptorPool")
            .field("max_sets", &self.max_sets)
            .finish()
    }
}

