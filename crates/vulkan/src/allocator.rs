//! GPU memory allocator -- direct Vulkan memory management.
//!
//! Originally targeted the `vma` crate, but that crate pins ash 0.37 while
//! this workspace standardizes on ash 0.38; carrying two copies of the Vulkan
//! types was unacceptable for an ultra-lightweight engine. This module
//! implements classic explicit allocation (find memory type -> allocate ->
//! bind) with the same ergonomic surface.

use ash::vk::Handle;
use ash::{vk, Device};

/// GPU memory allocator
pub struct GpuAllocator {
    /// Vulkan device
    pub device: ash::Device,
    /// Physical device
    pub physical_device: vk::PhysicalDevice,
    /// Memory properties cache
    memory_properties: vk::PhysicalDeviceMemoryProperties,
    /// Live allocation counter
    live_allocations: std::sync::atomic::AtomicUsize,
}

impl std::fmt::Debug for GpuAllocator {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("GpuAllocator")
            .field("physical_device", &self.physical_device.as_raw())
            .finish()
    }
}

/// High-level allocation intent flags.
#[derive(Clone, Copy, Debug, Default)]
pub struct AllocFlags(pub u32);

impl AllocFlags {
    /// Memory will be read/written by CPU (HOST_VISIBLE)
    pub const HOST_VISIBLE: Self = Self(1 << 0);
    /// Prefer persistent mapping
    pub const MAPPED: Self = Self(1 << 1);
    /// Device local memory (GPU only)
    pub const DEVICE_LOCAL: Self = Self(1 << 2);
}

/// A bound piece of device memory.
#[derive(Clone, Copy, Debug)]
pub struct Allocation {
    pub memory: vk::DeviceMemory,
    pub offset: vk::DeviceSize,
    pub size: vk::DeviceSize,
    pub mapped: *mut std::ffi::c_void,
    /// True when the backing memory type is host coherent (no flush needed).
    pub coherent: bool,
}

unsafe impl Send for Allocation {}
unsafe impl Sync for Allocation {}

impl Default for Allocation {
    fn default() -> Self {
        Self {
            memory: vk::DeviceMemory::null(),
            offset: 0,
            size: 0,
            mapped: std::ptr::null_mut(),
            coherent: false,
        }
    }
}

/// Snapshot of an allocation's properties.
#[derive(Debug, Clone)]
pub struct AllocationInfo {
    pub memory_type: u32,
    pub size: u64,
    pub offset: u64,
    pub mapped_data: *mut std::ffi::c_void,
}

impl GpuAllocator {
    /// Create a new allocator
    pub fn new(
        device: &Device,
        physical_device: vk::PhysicalDevice,
        instance: &ash::Instance,
    ) -> Result<Self, String> {
        let memory_properties = unsafe { instance.get_physical_device_memory_properties(physical_device) };
        Ok(Self {
            device: device.clone(),
            physical_device,
            memory_properties,
            live_allocations: std::sync::atomic::AtomicUsize::new(0),
        })
    }

    fn find_type(&self, type_bits: u32, properties: vk::MemoryPropertyFlags) -> Option<u32> {
        for i in 0..self.memory_properties.memory_type_count {
            if (type_bits & (1 << i)) != 0
                && self.memory_properties.memory_types[i as usize]
                    .property_flags
                    .contains(properties)
            {
                return Some(i);
            }
        }
        None
    }

    fn alloc_memory(
        &mut self,
        requirements: vk::MemoryRequirements,
        flags: AllocFlags,
        required_flags: vk::MemoryPropertyFlags,
        preferred_flags: vk::MemoryPropertyFlags,
    ) -> Result<Allocation, String> {
        // Decide target property mask from intent + caller hints.
        let mut wanted = required_flags;
        if wanted.is_empty() {
            if flags.0 & AllocFlags::HOST_VISIBLE.0 != 0 {
                wanted = vk::MemoryPropertyFlags::HOST_VISIBLE
                    | vk::MemoryPropertyFlags::HOST_COHERENT;
                if flags.0 & AllocFlags::DEVICE_LOCAL.0 != 0 {
                    wanted |= vk::MemoryPropertyFlags::DEVICE_LOCAL;
                }
            } else if flags.0 & AllocFlags::DEVICE_LOCAL.0 != 0 {
                wanted = vk::MemoryPropertyFlags::DEVICE_LOCAL;
            } else {
                wanted = preferred_flags;
            }
        }

        let strict = required_flags;
        let type_index = self
            .find_type(requirements.memory_type_bits, wanted)
            // Fall back: any type satisfying hard requirements, preferring soft ones.
            .or_else(|| self.find_type(requirements.memory_type_bits, strict))
            .ok_or("no suitable Vulkan memory type found")?;

        let info = vk::MemoryAllocateInfo {
            allocation_size: requirements.size,
            memory_type_index: type_index,
            ..Default::default()
        };
        let memory = unsafe {
            self.device
                .allocate_memory(&info, None)
                .map_err(|e| format!("allocate_memory failed: {:?}", e))?
        };

        let mem_type = self.memory_properties.memory_types[type_index as usize];
        let coherent = mem_type
            .property_flags
            .contains(vk::MemoryPropertyFlags::HOST_COHERENT);

        self.live_allocations
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        Ok(Allocation {
            memory,
            offset: 0,
            size: requirements.size,
            mapped: std::ptr::null_mut(),
            coherent,
        })
    }

    /// Allocate + bind a buffer. Returns handle and its memory record.
    pub fn allocate_buffer(
        &mut self,
        size: u64,
        usage: vk::BufferUsageFlags,
        flags: AllocFlags,
        required_flags: vk::MemoryPropertyFlags,
        preferred_flags: vk::MemoryPropertyFlags,
    ) -> Result<(vk::Buffer, Allocation), String> {
        let create_info = vk::BufferCreateInfo {
            size,
            usage,
            sharing_mode: vk::SharingMode::EXCLUSIVE,
            ..Default::default()
        };
        let buffer = unsafe {
            self.device
                .create_buffer(&create_info, None)
                .map_err(|e| format!("create_buffer failed: {:?}", e))?
        };
        let requirements =
            unsafe { self.device.get_buffer_memory_requirements(buffer) };

        let mut allocation =
            match self.alloc_memory(requirements, flags, required_flags, preferred_flags) {
                Ok(a) => a,
                Err(e) => {
                    unsafe { self.device.destroy_buffer(buffer, None) };
                    return Err(e);
                }
            };

        unsafe {
            self.device
                .bind_buffer_memory(buffer, allocation.memory, 0)
                .map_err(|e| format!("bind_buffer_memory failed: {:?}", e))?;
        }
        allocation.size = size;

        if flags.0 & AllocFlags::MAPPED.0 != 0 || flags.0 & AllocFlags::HOST_VISIBLE.0 != 0 {
            let map_size = allocation.size;
            self.map_memory(&mut allocation, map_size, 0)?;
        }
        Ok((buffer, allocation))
    }

    /// Allocate + bind an image and wrap it in a view.
    #[allow(clippy::too_many_arguments)]
    pub fn allocate_image(
        &mut self,
        extent: [u32; 3],
        format: vk::Format,
        usage: vk::ImageUsageFlags,
        flags: AllocFlags,
        mip_levels: u32,
        array_layers: u32,
    ) -> Result<(vk::Image, vk::ImageView, Allocation), String> {
        let image_info = vk::ImageCreateInfo {
            image_type: vk::ImageType::TYPE_2D,
            format,
            extent: vk::Extent3D {
                width: extent[0].max(1),
                height: extent[1].max(1),
                depth: extent[2].max(1),
            },
            mip_levels,
            array_layers,
            samples: vk::SampleCountFlags::TYPE_1,
            tiling: vk::ImageTiling::OPTIMAL,
            usage,
            sharing_mode: vk::SharingMode::EXCLUSIVE,
            initial_layout: vk::ImageLayout::UNDEFINED,
            ..Default::default()
        };
        let image = unsafe {
            self.device
                .create_image(&image_info, None)
                .map_err(|e| format!("create_image failed: {:?}", e))?
        };
        let requirements = unsafe { self.device.get_image_memory_requirements(image) };

        let mut allocation =
            match self.alloc_memory(requirements, flags, vk::MemoryPropertyFlags::empty(), vk::MemoryPropertyFlags::empty()) {
                Ok(a) => a,
                Err(e) => {
                    unsafe { self.device.destroy_image(image, None) };
                    return Err(e);
                }
            };

        unsafe {
            self.device
                .bind_image_memory(image, allocation.memory, 0)
                .map_err(|e| format!("bind_image_memory failed: {:?}", e))?;
        }
        allocation.size = requirements.size;

        let view_info = vk::ImageViewCreateInfo {
            image,
            view_type: vk::ImageViewType::TYPE_2D,
            format,
            subresource_range: vk::ImageSubresourceRange {
                aspect_mask: vk::ImageAspectFlags::COLOR,
                base_mip_level: 0,
                level_count: mip_levels,
                base_array_layer: 0,
                layer_count: array_layers,
            },
            ..Default::default()
        };
        let view = unsafe {
            self.device
                .create_image_view(&view_info, None)
                .map_err(|e| format!("create_image_view failed: {:?}", e))?
        };
        Ok((image, view, allocation))
    }

    /// Map memory for host access. Returns pointer offset into the mapping.
    pub fn map_memory(
        &mut self,
        allocation: &mut Allocation,
        _size: u64,
        offset: u64,
    ) -> Result<*mut std::ffi::c_void, String> {
        if allocation.mapped.is_null() {
            allocation.mapped = unsafe {
                self.device
                    .map_memory(
                        allocation.memory,
                        allocation.offset,
                        allocation.size,
                        vk::MemoryMapFlags::empty(),
                    )
                    .map_err(|e| format!("map_memory failed: {:?}", e))?
            };
        }
        Ok(unsafe { allocation.mapped.add(offset as usize) })
    }

    /// Unmap memory (persistent mappings stay until explicitly unmapped).
    pub fn unmap_memory(&mut self, allocation: &mut Allocation) -> Result<(), String> {
        if !allocation.mapped.is_null() {
            unsafe { self.device.unmap_memory(allocation.memory) };
            allocation.mapped = std::ptr::null_mut();
        }
        Ok(())
    }

    /// Flush mapped memory range (host -> device visibility).
    /// No-op on host-coherent memory.
    pub fn flush_allocation(
        &self,
        allocation: &Allocation,
        offset: u64,
        size: u64,
    ) -> Result<(), String> {
        if allocation.coherent || allocation.mapped.is_null() {
            return Ok(());
        }
        let range = vk::MappedMemoryRange {
            memory: allocation.memory,
            offset: allocation.offset + offset,
            size: if size == 0 { vk::WHOLE_SIZE } else { size },
            ..Default::default()
        };
        unsafe {
            self.device
                .flush_mapped_memory_ranges(&[range])
                .map_err(|e| format!("flush failed: {:?}", e))
        }
    }

    /// Invalidate mapped memory range (device -> host visibility).
    pub fn invalidate_allocation(
        &self,
        allocation: &Allocation,
        offset: u64,
        size: u64,
    ) -> Result<(), String> {
        if allocation.coherent || allocation.mapped.is_null() {
            return Ok(());
        }
        let range = vk::MappedMemoryRange {
            memory: allocation.memory,
            offset: allocation.offset + offset,
            size: if size == 0 { vk::WHOLE_SIZE } else { size },
            ..Default::default()
        };
        unsafe {
            self.device
                .invalidate_mapped_memory_ranges(&[range])
                .map_err(|e| format!("invalidate failed: {:?}", e))
        }
    }

    /// Introspect an allocation.
    pub fn allocation_info(&self, allocation: &Allocation) -> AllocationInfo {
        AllocationInfo {
            memory_type: 0,
            size: allocation.size,
            offset: allocation.offset,
            mapped_data: allocation.mapped,
        }
    }

    /// Free a buffer allocation (destroys buffer + memory together)
    pub fn free_buffer(&mut self, buffer: vk::Buffer, allocation: &mut Allocation) {
        unsafe {
            self.device.destroy_buffer(buffer, None);
            if allocation.memory != vk::DeviceMemory::null() {
                self.device.free_memory(allocation.memory, None);
            }
        }
        *allocation = Allocation::default();
        self.live_allocations
            .fetch_sub(1, std::sync::atomic::Ordering::Relaxed);
    }

    /// Free an image allocation (destroys view, image + memory together)
    pub fn free_image(
        &mut self,
        image: vk::Image,
        view: vk::ImageView,
        allocation: &mut Allocation,
    ) {
        unsafe {
            self.device.destroy_image_view(view, None);
            self.device.destroy_image(image, None);
            if allocation.memory != vk::DeviceMemory::null() {
                self.device.free_memory(allocation.memory, None);
            }
        }
        *allocation = Allocation::default();
        self.live_allocations
            .fetch_sub(1, std::sync::atomic::Ordering::Relaxed);
    }

    /// Get memory type index for given properties
    pub fn find_memory_type(
        &self,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Option<u32> {
        self.find_type(type_filter, properties)
    }

    /// Get device pointer
    pub fn device(&self) -> &ash::Device {
        &self.device
    }

    /// Number of live allocations (profiler feeds on this).
    pub fn live_allocations(&self) -> usize {
        self.live_allocations.load(std::sync::atomic::Ordering::Relaxed)
    }
}

// Compatibility alias used across the crate
pub type MemoryAllocator = GpuAllocator;

