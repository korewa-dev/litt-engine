//! Minimal custom memory allocator.
//! Falls back to Vulkan's default allocator for simplicity.
//! In production, replace with VMA (Vulkan Memory Allocator).

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};

/// Allocation flag
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct AllocationFlags(pub u32);

impl AllocationFlags {
    pub const HOST_VISIBLE: Self = Self(vk::MemoryPropertyFlags::HOST_VISIBLE as u32);
    pub const HOST_COHERENT: Self = Self(vk::MemoryPropertyFlags::HOST_COHERENT as u32);
    pub const DEVICE_LOCAL: Self = Self(vk::MemoryPropertyFlags::DEVICE_LOCAL as u32);
}

/// Memory allocation result
#[derive(Debug)]
pub struct Allocation {
    pub memory: vk::DeviceMemory,
    pub size: u64,
    pub offset: u64,
}

/// A simple memory allocator that tracks allocations
pub struct MemoryAllocator {
    device: ash::Device,
    physical_device: vk::PhysicalDevice,
    memory_properties: vk::PhysicalDeviceMemoryProperties,
    allocations: Vec<Allocation>,
    total_allocated: u64,
}

impl MemoryAllocator {
    pub fn new(device: &Device, physical: vk::PhysicalDevice, mem_props: &vk::PhysicalDeviceMemoryProperties) -> Self {
        Self {
            device: device.clone(),
            physical_device: physical,
            memory_properties: *mem_props,
            allocations: Vec::new(),
            total_allocated: 0,
        }
    }

    /// Allocate device-local memory for a buffer
    pub fn allocate_buffer(
        &mut self,
        size: u64,
        usage: vk::BufferUsageFlags,
    ) -> Result<Allocation, String> {
        // For simplicity, create buffer with default allocator
        let buffer_info = vk::BufferCreateInfo::builder()
            .size(size)
            .usage(usage)
            .sharing_mode(vk::SharingMode::EXCLUSIVE)
            .build();

        let buffer = unsafe { self.device.create_buffer(&buffer_info, None)
            .map_err(|e| format!("Buffer allocation failed: {:?}", e))? };

        // Get memory requirements
        let requirements = unsafe { self.device.buffer_memory_requirements(buffer) };

        // Find suitable memory type
        let mem_type_index = self.find_memory_type(
            requirements.memory_type_bits,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        ).ok_or("Failed to find suitable memory type")?;

        // Allocate memory
        let alloc_info = vk::AllocationInfo::default(); // placeholder
        let memory_info = vk::MemoryAllocateInfo::builder()
            .allocation_size(requirements.size)
            .memory_type_index(mem_type_index)
            .build();

        let memory = unsafe { self.device.allocate_memory(&memory_info, None)
            .map_err(|e| format!("Memory allocation failed: {:?}", e))? };

        // Bind
        unsafe { self.device.bind_buffer_memory(buffer, memory, 0)
            .map_err(|e| format!("Buffer memory binding failed: {:?}", e))? };

        let alloc = Allocation {
            memory,
            size,
            offset: 0,
        };
        self.allocations.push(alloc.clone());
        self.total_allocated += size;

        Ok(alloc)
    }

    /// Allocate device-local memory for an image
    pub fn allocate_image(
        &mut self,
        extent: [u32; 3],
        format: vk::Format,
        usage: vk::ImageUsageFlags,
        mip_levels: u32,
    ) -> Result<Allocation, String> {
        let image_info = vk::ImageCreateInfo::builder()
            .image_type(vk::ImageType::TYPE_2D)
            .format(format)
            .extent(vk::Extent3D {
                width: extent[0],
                height: extent[1],
                depth: extent[2],
            })
            .mip_levels(mip_levels)
            .array_layers(1)
            .samples(vk::SampleCountFlags::TYPE_1)
            .tiling(vk::ImageTiling::OPTIMAL)
            .usage(usage)
            .sharing_mode(vk::SharingMode::EXCLUSIVE)
            .initial_layout(vk::ImageLayout::UNDEFINED)
            .build();

        let image = unsafe { self.device.create_image(&image_info, None)
            .map_err(|e| format!("Image allocation failed: {:?}", e))? };

        let requirements = unsafe { self.device.image_memory_requirements(image) };
        let mem_type_index = self.find_memory_type(
            requirements.memory_type_bits,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        ).ok_or("Failed to find image memory type")?;

        let memory_info = vk::MemoryAllocateInfo::builder()
            .allocation_size(requirements.size)
            .memory_type_index(mem_type_index)
            .build();

        let memory = unsafe { self.device.allocate_memory(&memory_info, None)
            .map_err(|e| format!("Image memory allocation failed: {:?}", e))? };

        unsafe { self.device.bind_image_memory(image, memory, 0)
            .map_err(|e| format!("Image memory binding failed: {:?}", e))? };

        let alloc = Allocation {
            memory,
            size: requirements.size,
            offset: 0,
        };
        self.allocations.push(alloc.clone());
        self.total_allocated += requirements.size;

        Ok(alloc)
    }

    /// Free an allocation
    pub fn free(&mut self, alloc: &Allocation) {
        unsafe {
            self.device.free_memory(alloc.memory, None);
        }
        self.allocations.retain(|a| a.memory != alloc.memory);
        self.total_allocated = self.total_allocated.saturating_sub(alloc.size);
    }

    /// Clean up all allocations
    pub fn free_all(&mut self) {
        for alloc in self.allocations.drain(..) {
            self.free(&alloc);
        }
        self.total_allocated = 0;
    }

    fn find_memory_type(&self, type_filter: u32, properties: vk::MemoryPropertyFlags) -> Option<u32> {
        for i in 0..self.memory_properties.memory_type_count {
            if (type_filter & (1 << i)) != 0
                && self.memory_properties.memory_types[i as usize].property_flags.contains(properties)
            {
                return Some(i);
            }
        }
        None
    }
}

impl Drop for MemoryAllocator {
    fn drop(&mut self) {
        self.free_all();
    }
}
