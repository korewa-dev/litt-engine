//! Vulkan Memory Allocator (VMA) integration.
//! High-performance memory management for Vulkan buffers and images.

use ash::{vk, Device};
use bytemuck::{Pod, Zeroable};
use vma::Allocator;

/// VMA-based memory allocator
#[derive(Debug)]
pub struct VmaAllocator {
    /// Raw VMA allocator
    pub allocator: Allocator,
    /// Vulkan device for queries
    pub device: ash::Device,
    /// Physical device
    pub physical_device: vk::PhysicalDevice,
    /// Memory properties for queries
    pub memory_properties: vk::PhysicalDeviceMemoryProperties,
}

/// Allocation flags for VMA
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocFlags(pub u32);

impl AllocFlags {
    /// Memory will be read by CPU (HOST_VISIBLE)
    pub const HOST_VISIBLE: Self = Self(1 << 0);
    /// Host coherent (no flush invalid required)
    pub const HOST_COHERENT: Self = Self(1 << 1);
    /// Device local memory (GPU only)
    pub const DEVICE_LOCAL: Self = Self(1 << 2);
    /// Prefer device local
    pub const PREFER_DEVICE_LOCAL: Self = Self(1 << 3);
}

/// Allocation info for buffer/image creation
#[derive(Debug, Clone)]
pub struct AllocationInfo {
    pub memory_type: u32,
    pub size: u64,
    pub aligned_size: u64,
    pub allocation: Option<vma::Allocation>,
}

impl VmaAllocator {
    /// Create a new VMA allocator
    pub fn new(
        device: &Device,
        physical_device: vk::PhysicalDevice,
        instance: &ash::Instance,
    ) -> Result<Self, String> {
        unsafe {
            let mem_props = instance.physical_device_memory_properties(physical_device);
            let allocation_callbacks = vma::AllocationCallbacks {
                p_user_data: std::ptr::null_mut(),
                pfn_allocate: None,
                pfn_free: None,
                pfn_reallocation: None,
                pfn_copy: None,
                pfn_flush_mapped_range: None,
                pfn_invalidate_mapped_range: None,
            };

            let allocator_info = vma::AllocatorCreateInfo {
                flags: vma::AllocatorCreateFlags::empty(),
                physical_device,
                device: *device.handle(),
                instance: *instance.handle(),
                debug_name: std::ptr::null(),
                allocation_callbacks,
                preferred_local_device_power_state: vma::PhysicalDevicePowerState::DEFAULT,
                explicit_external_memory_handle_types: vk::ExternalMemoryHandleTypeFlags::empty(),
            };

            let allocator = Allocator::new(device, &allocator_info)
                .map_err(|e| format!("Failed to create VMA allocator: {:?}", e))?;

            Ok(Self {
                allocator,
                device: device.clone(),
                physical_device,
                memory_properties: mem_props,
            })
        }
    }

    /// Allocate a buffer with VMA
    pub fn allocate_buffer(
        &mut self,
        size: u64,
        usage: vk::BufferUsageFlags,
        flags: AllocFlags,
        required_flags: vk::MemoryPropertyFlags,
        preferred_flags: vk::MemoryPropertyFlags,
    ) -> Result<(vk::Buffer, vma::Allocation), String> {
        unsafe {
            let buffer_info = vk::BufferCreateInfo::builder()
                .size(size)
                .usage(usage)
                .sharing_mode(vk::SharingMode::EXCLUSIVE)
                .build();

            let buffer = self.device
                .create_buffer(&buffer_info, None)
                .map_err(|e| format!("Buffer creation failed: {:?}", e))?;

            let requirements = self.device.buffer_memory_requirements(buffer);

            let mut alloc_flags = vma::AllocationCreateFlags::empty();
            if flags.0 & AllocFlags::HOST_VISIBLE.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::HOST_VISIBLE;
            }
            if flags.0 & AllocFlags::HOST_COHERENT.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::HOST_COHERENT;
            }
            if flags.0 & AllocFlags::PREFER_DEVICE_LOCAL.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::PREFER_DEVICE_LOCAL;
            }

            let alloc_info = vma::AllocationInfo {
                memory_type: 0,
                size,
                allocated_size: 0,
                mapped_range_offset: 0,
                mapped_range_size: 0,
            };

            let alloc = self.allocator
                .allocate(&requirements, &alloc_info, alloc_flags)
                .map_err(|e| format!("Buffer allocation failed: {:?}", e))?;

            let mem_type_index = alloc.memory_type();
            let mem_type = self.memory_properties.memory_types[mem_type_index as usize];
            
            // Verify memory properties match requirements
            let has_required = mem_type.property_flags.contains(required_flags);
            let has_preferred = mem_type.property_flags.contains(preferred_flags);
            
            if !has_required && !has_preferred {
                // Try to reallocate with correct flags
                self.allocator.free(&alloc).ok();
                self.device.destroy_buffer(buffer, None);
                return Err("Could not find suitable memory type for buffer".to_string());
            }

            self.device.bind_buffer_memory(
                buffer,
                alloc.memory(),
                alloc.offset(),
            ).map_err(|e| format!("Buffer memory binding failed: {:?}", e))?;

            Ok((buffer, alloc))
        }
    }

    /// Allocate an image with VMA
    pub fn allocate_image(
        &mut self,
        extent: [u32; 3],
        format: vk::Format,
        usage: vk::ImageUsageFlags,
        flags: AllocFlags,
        required_flags: vk::MemoryPropertyFlags,
        preferred_flags: vk::MemoryPropertyFlags,
        mip_levels: u32,
        array_layers: u32,
    ) -> Result<(vk::Image, vk::ImageView, vma::Allocation), String> {
        unsafe {
            let image_info = vk::ImageCreateInfo::builder()
                .image_type(vk::ImageType::TYPE_2D)
                .format(format)
                .extent(vk::Extent3D {
                    width: extent[0],
                    height: extent[1],
                    depth: extent[2],
                })
                .mip_levels(mip_levels)
                .array_layers(array_layers)
                .samples(vk::SampleCountFlags::TYPE_1)
                .tiling(vk::ImageTiling::OPTIMAL)
                .usage(usage)
                .sharing_mode(vk::SharingMode::EXCLUSIVE)
                .initial_layout(vk::ImageLayout::UNDEFINED)
                .build();

            let image = self.device
                .create_image(&image_info, None)
                .map_err(|e| format!("Image creation failed: {:?}", e))?;

            let requirements = self.device.image_memory_requirements(image);

            let mut alloc_flags = vma::AllocationCreateFlags::empty();
            if flags.0 & AllocFlags::HOST_VISIBLE.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::HOST_VISIBLE;
            }
            if flags.0 & AllocFlags::HOST_COHERENT.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::HOST_COHERENT;
            }
            if flags.0 & AllocFlags::PREFER_DEVICE_LOCAL.0 != 0 {
                alloc_flags |= vma::AllocationCreateFlags::PREFER_DEVICE_LOCAL;
            }

            let alloc_info = vma::AllocationInfo {
                memory_type: 0,
                size: requirements.size,
                allocated_size: 0,
                mapped_range_offset: 0,
                mapped_range_size: 0,
            };

            let alloc = self.allocator
                .allocate(&requirements, &alloc_info, alloc_flags)
                .map_err(|e| format!("Image allocation failed: {:?}", e))?;

            let mem_type_index = alloc.memory_type();
            let mem_type = self.memory_properties.memory_types[mem_type_index as usize];
            
            // Verify memory properties
            let has_required = mem_type.property_flags.contains(required_flags);
            let has_preferred = mem_type.property_flags.contains(preferred_flags);
            
            if !has_required && !has_preferred {
                self.allocator.free(&alloc).ok();
                self.device.destroy_image(image, None);
                return Err("Could not find suitable memory type for image".to_string());
            }

            self.device.bind_image_memory(
                image,
                alloc.memory(),
                alloc.offset(),
            ).map_err(|e| format!("Image memory binding failed: {:?}", e))?;

            // Create image view
            let view_info = vk::ImageViewCreateInfo::builder()
                .image(image)
                .view_type(vk::ImageViewType::TYPE_2D)
                .format(format)
                .subresource_range(vk::ImageSubresourceRange {
                    aspect_mask: match format {
                        vk::Format::R32G32B32A32_SFLOAT | 
                        vk::Format::R16G16B16A16_SFLOAT |
                        vk::Format::R8G8B8A8_UNORM |
                        vk::Format::R32_SFLOAT => vk::ImageAspectFlags::COLOR,
                        _ => vk::ImageAspectFlags::COLOR,
                    },
                    level_count: mip_levels,
                    layer_count: array_layers,
                    ..Default::default()
                })
                .build();

            let view = self.device
                .create_image_view(&view_info, None)
                .map_err(|e| format!("Image view creation failed: {:?}", e))?;

            Ok((image, view, alloc))
        }
    }

    /// Map memory for host access
    pub fn map_memory(
        &self,
        allocation: &vma::Allocation,
        size: u64,
        offset: u64,
    ) -> Result<*mut std::ffi::c_void, String> {
        unsafe {
            let ptr = self.allocator
                .map(allocation)
                .map_err(|e| format!("Failed to map memory: {:?}", e))?;
            Ok(ptr.add(offset as usize) as *mut std::ffi::c_void)
        }
    }

    /// Unmap memory
    pub fn unmap_memory(&self, allocation: &vma::Allocation) -> Result<(), String> {
        unsafe {
            self.allocator.unmap(allocation)
                .map_err(|e| format!("Failed to unmap memory: {:?}", e))?;
            Ok(())
        }
    }

    /// Flush mapped memory range
    pub fn flush_allocation(
        &self,
        allocation: &vma::Allocation,
        offset: u64,
        size: u64,
    ) -> Result<(), String> {
        unsafe {
            self.allocator.flush(allocation, offset, size)
                .map_err(|e| format!("Failed to flush allocation: {:?}", e))?;
            Ok(())
        }
    }

    /// Invalidate mapped memory range
    pub fn invalidate_allocation(
        &self,
        allocation: &vma::Allocation,
        offset: u64,
        size: u64,
    ) -> Result<(), String> {
        unsafe {
            self.allocator.invalidate(allocation, offset, size)
                .map_err(|e| format!("Failed to invalidate allocation: {:?}", e))?;
            Ok(())
        }
    }

    /// Free a buffer allocation
    pub fn free_buffer(
        &mut self,
        buffer: vk::Buffer,
        allocation: vma::Allocation,
    ) {
        unsafe {
            self.device.destroy_buffer(buffer, None);
            self.allocator.free(&allocation);
        }
    }

    /// Free an image allocation
    pub fn free_image(
        &mut self,
        image: vk::Image,
        view: vk::ImageView,
        allocation: vma::Allocation,
    ) {
        unsafe {
            self.device.destroy_image_view(view, None);
            self.device.destroy_image(image, None);
            self.allocator.free(&allocation);
        }
    }

    /// Get memory type index for given properties
    pub fn find_memory_type(
        &self,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Option<u32> {
        for i in 0..self.memory_properties.memory_type_count {
            if (type_filter & (1 << i)) != 0
                && self.memory_properties.memory_types[i as usize].property_flags.contains(properties)
            {
                return Some(i);
            }
        }
        None
    }

    /// Get device pointer for VMA
    pub fn device(&self) -> &ash::Device {
        &self.device
    }

    /// Get allocator
    pub fn allocator(&self) -> &Allocator {
        &self.allocator
    }
}

impl Drop for VmaAllocator {
    fn drop(&mut self) {
        // VMA cleanup is handled by the allocator
    }
}

// Re-export vma types for convenience
pub use vma::{Allocation, AllocationCreateFlags, Allocator, AllocatorCreateInfo};

// =============================================================================
// Legacy allocator wrapper (for compatibility)
// =============================================================================

/// Legacy allocation handle
#[derive(Clone, Copy, Debug)]
pub struct AllocatorHandle {
    pub device_ptr: u64,
}

// Backward compatibility type alias
pub type MemoryAllocator = VmaAllocator;
