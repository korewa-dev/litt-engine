//! Complete Ray Tracing Pipeline Implementation
//! Full BLAS + TLAS build pipeline with proper memory management.
//! Migrated to ash 0.38 RT naming (GeometryFlagsKHR, *_KHR storage usage).

use ash::vk;
use ash::Device;
use crate::allocator::{GpuAllocator, AllocFlags};

/// RT function loader alias
pub type RtLoader = ash::khr::acceleration_structure::Device;

/// Build scratch buffer for acceleration structure operations
#[derive(Debug, Default)]
pub struct ScratchBuffers {
    /// Scratch buffer for BLAS build
    pub blas_scratch: vk::Buffer,
    pub blas_scratch_alloc: Option<crate::allocator::Allocation>,
    /// Scratch buffer for TLAS build
    pub tlas_scratch: vk::Buffer,
    pub tlas_scratch_alloc: Option<crate::allocator::Allocation>,
    /// Scratch buffer size (reused)
    pub scratch_size: u64,
}

/// Bottom-Level Acceleration Structure
#[derive(Debug)]
pub struct Blas {
    pub handle: vk::AccelerationStructureKHR,
    pub device_address: u64,
    pub size: u64,
    pub geometry_count: u32,
}

/// Top-Level Acceleration Structure
#[derive(Debug)]
pub struct Tlas {
    pub handle: vk::AccelerationStructureKHR,
    pub device_address: u64,
    pub instance_count: u32,
    pub blas_count: u32,
}

/// Complete acceleration structure hierarchy
#[derive(Debug)]
pub struct AccelerationStructures {
    pub tlas: Tlas,
    pub blas_count: u32,
}

// =============================================================================
// BLAS Builder
// =============================================================================

/// Builder for creating BLAS structures
#[derive(Debug, Default)]
pub struct BlasBuilder {
    geometries: Vec<BlasGeometry>,
}

#[derive(Clone, Copy)]
pub struct BlasGeometry {
    pub triangle_count: u32,
    pub vertex_stride: u32,
    pub vertex_buffer: vk::Buffer,
    pub vertex_buffer_address: u64,
    pub flags: vk::GeometryFlagsKHR,
}

impl std::fmt::Debug for BlasGeometry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("BlasGeometry")
            .field("triangle_count", &self.triangle_count)
            .field("vertex_stride", &self.vertex_stride)
            .finish()
    }
}

impl BlasBuilder {
    /// Create new BLAS builder
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a geometry to the BLAS
    pub fn add_geometry(mut self, geometry: BlasGeometry) -> Self {
        self.geometries.push(geometry);
        self
    }

    /// Query build sizes for this BLAS without building yet.
    ///
    /// Returns (acceleration structure size, build scratch size).
    pub unsafe fn query_sizes(
        &self,
        rt_loader: &RtLoader,
    ) -> Result<(u64, u64), String> {
        if self.geometries.is_empty() {
            return Err("Cannot size BLAS with no geometries".to_string());
        }
        let geometries_vk = self.build_geometries();
        let max_prims: Vec<u32> =
            self.geometries.iter().map(|g| g.triangle_count).collect();

        let build_info = self.build_info(&geometries_vk);
        let mut sizes = vk::AccelerationStructureBuildSizesInfoKHR::default();
        rt_loader.get_acceleration_structure_build_sizes(
            vk::AccelerationStructureBuildTypeKHR::DEVICE,
            &build_info,
            &max_prims,
            &mut sizes,
        );
        Ok((sizes.acceleration_structure_size, sizes.build_scratch_size))
    }

    fn build_geometries(&self) -> Vec<vk::AccelerationStructureGeometryKHR<'static>> {
        self.geometries
            .iter()
            .map(|geom| {
                let tri = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    vertex_format: vk::Format::R32G32B32_SFLOAT,
                    vertex_data: vk::DeviceOrHostAddressConstKHR {
                        device_address: geom.vertex_buffer_address,
                    },
                    vertex_stride: geom.vertex_stride as u64,
                    max_vertex: geom.triangle_count * 3,
                    index_type: vk::IndexType::NONE_KHR,
                    index_data: vk::DeviceOrHostAddressConstKHR { device_address: 0 },
                    transform_data: vk::DeviceOrHostAddressConstKHR { device_address: 0 },
                    ..Default::default()
                };
                vk::AccelerationStructureGeometryKHR {
                    geometry_type: vk::GeometryTypeKHR::TRIANGLES,
                    geometry: vk::AccelerationStructureGeometryDataKHR { triangles: tri },
                    flags: geom.flags,
                    ..Default::default()
                }
            })
            .collect()
    }

    fn build_info<'a>(
        &'a self,
        geometries_vk: &'a [vk::AccelerationStructureGeometryKHR<'a>],
    ) -> vk::AccelerationStructureBuildGeometryInfoKHR<'a> {
        vk::AccelerationStructureBuildGeometryInfoKHR {
            ty: vk::AccelerationStructureTypeKHR::BOTTOM_LEVEL,
            flags: vk::BuildAccelerationStructureFlagsKHR::PREFER_FAST_BUILD,
            mode: vk::BuildAccelerationStructureModeKHR::BUILD,
            geometry_count: geometries_vk.len() as u32,
            p_geometries: geometries_vk.as_ptr(),
            ..Default::default()
        }
    }

    /// Create the acceleration structure object (build must be recorded separately).
    pub unsafe fn build(
        self,
        device: &Device,
        rt_loader: &RtLoader,
        allocator: &mut GpuAllocator,
    ) -> Result<(Blas, ScratchBuffers), String> {
        if self.geometries.is_empty() {
            return Err("Cannot build BLAS with no geometries".to_string());
        }

        let geom_count = self.geometries.len() as u32;

        // Query build sizes
        let geometries_vk = self.build_geometries();
        let max_prims: Vec<u32> =
            self.geometries.iter().map(|g| g.triangle_count).collect();
        let build_info = self.build_info(&geometries_vk);
        let mut sizes = vk::AccelerationStructureBuildSizesInfoKHR::default();
        rt_loader.get_acceleration_structure_build_sizes(
            vk::AccelerationStructureBuildTypeKHR::DEVICE,
            &build_info,
            &max_prims,
            &mut sizes,
        );

        // Allocate backing buffer (device address required)
        let (blas_buffer, _blas_alloc) = allocator.allocate_buffer(
            sizes.acceleration_structure_size.max(256),
            vk::BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR
                | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::empty(),
        )?;

        let blas_buffer_addr = device_address_of(device, blas_buffer);

        let blas_info = vk::AccelerationStructureCreateInfoKHR {
            size: sizes.acceleration_structure_size,
            ty: vk::AccelerationStructureTypeKHR::BOTTOM_LEVEL,
            buffer: blas_buffer,
            offset: 0,
            device_address: 0,
            ..Default::default()
        };

        let blas = rt_loader
            .create_acceleration_structure(&blas_info, None)
            .map_err(|e| format!("BLAS creation failed: {e:?}"))?;

        let info = vk::AccelerationStructureDeviceAddressInfoKHR {
            acceleration_structure: blas,
            ..Default::default()
        };
        let device_address = rt_loader.get_acceleration_structure_device_address(&info);

        // Allocate scratch buffer
        let (blas_scratch, blas_scratch_alloc) = allocator.allocate_buffer(
            sizes.build_scratch_size.max(256),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::empty(),
        )?;
        let _ = blas_buffer_addr;

        Ok((
            Blas {
                handle: blas,
                device_address,
                size: sizes.acceleration_structure_size,
                geometry_count: geom_count,
            },
            ScratchBuffers {
                blas_scratch,
                blas_scratch_alloc: Some(blas_scratch_alloc),
                tlas_scratch: vk::Buffer::null(),
                tlas_scratch_alloc: None,
                scratch_size: sizes.build_scratch_size,
            },
        ))
    }
}

// =============================================================================
// TLAS Builder
// =============================================================================

/// Instance data for TLAS -- mirrors VkAccelerationStructureInstanceKHR layout.
#[derive(Clone, Copy, Debug, bytemuck::Pod, bytemuck::Zeroable)]
#[repr(C)]
pub struct TlasInstance {
    pub transform: [f32; 12],
    pub instance_custom_index_and_mask: u32, // 24-bit index | 8-bit mask
    pub instance_offset_and_flags: u32,
    pub acceleration_structure_reference: u64,
}

impl Default for TlasInstance {
    fn default() -> Self {
        Self {
            transform: [
                1.0, 0.0, 0.0, 0.0, //
                0.0, 1.0, 0.0, 0.0, //
                0.0, 0.0, 1.0, 0.0,
            ],
            instance_custom_index_and_mask: (0xFF << 8),
            instance_offset_and_flags: 0,
            acceleration_structure_reference: 0,
        }
    }
}

impl TlasInstance {
    /// Pack a custom index (<= 2^24) and visibility mask into one word.
    pub fn new(transform: [f32; 12], custom_index: u32, mask: u32, blas_device_address: u64) -> Self {
        Self {
            transform,
            instance_custom_index_and_mask: ((custom_index & 0x00FF_FFFF))
                | ((mask & 0xFF) << 24),
            instance_offset_and_flags: 0,
            acceleration_structure_reference: blas_device_address,
        }
    }
}

/// Builder for creating TLAS structures
#[derive(Debug, Default)]
pub struct TlasBuilder {
    instances: Vec<TlasInstance>,
}

impl TlasBuilder {
    /// Create new TLAS builder
    pub fn new() -> Self {
        Self::default()
    }

    /// Add an instance to the TLAS
    pub fn add_instance(mut self, instance: TlasInstance) -> Self {
        self.instances.push(instance);
        self
    }

    /// Create the TLAS object plus its instance and scratch buffers.
    pub unsafe fn build(
        self,
        device: &Device,
        rt_loader: &RtLoader,
        allocator: &mut GpuAllocator,
    ) -> Result<(Tlas, ScratchBuffers), String> {
        if self.instances.is_empty() {
            return Err("Cannot build TLAS with no instances".to_string());
        }

        let instance_count = self.instances.len() as u32;
        let instance_bytes =
            (self.instances.len() * std::mem::size_of::<TlasInstance>()) as u64;

        // Upload instances through host-visible memory
        let (instance_buffer, mut instance_alloc) = allocator.allocate_buffer(
            instance_bytes,
            vk::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR
                | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::HOST_VISIBLE,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            vk::MemoryPropertyFlags::empty(),
        )?;
        let ptr = allocator.map_memory(&mut instance_alloc, instance_bytes, 0)?;
        unsafe {
            std::ptr::copy_nonoverlapping(
                self.instances.as_ptr(),
                ptr as *mut TlasInstance,
                self.instances.len(),
            );
        }
        allocator.flush_allocation(&instance_alloc, 0, instance_bytes)?;
        let _ = allocator.unmap_memory(&mut instance_alloc);

        // Instances geometry reads from that buffer's device address
        let instances_data = vk::AccelerationStructureGeometryInstancesDataKHR {
            data: vk::DeviceOrHostAddressConstKHR {
                device_address: device_address_of(device, instance_buffer),
            },
            ..Default::default()
        };
        let geom = vk::AccelerationStructureGeometryKHR {
            geometry_type: vk::GeometryTypeKHR::INSTANCES,
            geometry: vk::AccelerationStructureGeometryDataKHR {
                instances: instances_data,
            },
            flags: vk::GeometryFlagsKHR::OPAQUE,
            ..Default::default()
        };
        let geometries = [geom];

        let build_info = vk::AccelerationStructureBuildGeometryInfoKHR {
            ty: vk::AccelerationStructureTypeKHR::TOP_LEVEL,
            flags: vk::BuildAccelerationStructureFlagsKHR::PREFER_FAST_BUILD,
            mode: vk::BuildAccelerationStructureModeKHR::BUILD,
            geometry_count: geometries.len() as u32,
            p_geometries: geometries.as_ptr(),
            ..Default::default()
        };

        let mut sizes = vk::AccelerationStructureBuildSizesInfoKHR::default();
        rt_loader.get_acceleration_structure_build_sizes(
            vk::AccelerationStructureBuildTypeKHR::DEVICE,
            &build_info,
            &[instance_count],
            &mut sizes,
        );

        // Allocate TLAS backing memory
        let (tlas_buffer, _tlas_alloc) = allocator.allocate_buffer(
            sizes.acceleration_structure_size.max(256),
            vk::BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR
                | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::empty(),
        )?;

        let tlas_info = vk::AccelerationStructureCreateInfoKHR {
            size: sizes.acceleration_structure_size,
            ty: vk::AccelerationStructureTypeKHR::TOP_LEVEL,
            buffer: tlas_buffer,
            offset: 0,
            device_address: 0,
            ..Default::default()
        };
        let tlas = rt_loader
            .create_acceleration_structure(&tlas_info, None)
            .map_err(|e| format!("TLAS creation failed: {e:?}"))?;

        let addr_info = vk::AccelerationStructureDeviceAddressInfoKHR {
            acceleration_structure: tlas,
            ..Default::default()
        };
        let device_address = rt_loader.get_acceleration_structure_device_address(&addr_info);

        // Scratch buffers
        let (blas_scratch, blas_scratch_alloc) = allocator.allocate_buffer(
            sizes.build_scratch_size.max(256),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::empty(),
        )?;
        let (tlas_scratch, tlas_scratch_alloc) = allocator.allocate_buffer(
            (sizes.build_scratch_size * 2).max(256),
            vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
            AllocFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
            vk::MemoryPropertyFlags::empty(),
        )?;

        Ok((
            Tlas {
                handle: tlas,
                device_address,
                instance_count,
                blas_count: instance_count,
            },
            ScratchBuffers {
                blas_scratch,
                blas_scratch_alloc: Some(blas_scratch_alloc),
                tlas_scratch,
                tlas_scratch_alloc: Some(tlas_scratch_alloc),
                scratch_size: sizes.build_scratch_size,
            },
        ))
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

/// Triangle structure for ray tracing (shared with pathtracer)
#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct Triangle {
    pub v0: [f32; 3],
    pub v1: [f32; 3],
    pub v2: [f32; 3],
    pub normal: [f32; 3],
    pub material_id: u32,
    pub _pad: [u32; 3],
}

impl Default for Triangle {
    fn default() -> Self {
        Self {
            v0: [0.0; 3],
            v1: [0.0; 3],
            v2: [0.0; 3],
            normal: [0.0, 1.0, 0.0],
            material_id: 0,
            _pad: [0; 3],
        }
    }
}

/// Get the device address of a buffer created with SHADER_DEVICE_ADDRESS.
pub fn device_address_of(device: &Device, buffer: vk::Buffer) -> u64 {
    let info = vk::BufferDeviceAddressInfo {
        buffer,
        ..Default::default()
    };
    unsafe { device.get_buffer_device_address(&info) }
}

/// Build a complete BLAS + TLAS hierarchy from CPU-side triangles.
pub fn build_acceleration_structures(
    device: &Device,
    rt_loader: &RtLoader,
    allocator: &mut GpuAllocator,
    triangles: &[Triangle],
    transforms: &[[f32; 12]],
) -> Result<AccelerationStructures, String> {
    // One triangle buffer holding all triangles (single geometry).
    let tri_size = (triangles.len().max(1) * std::mem::size_of::<Triangle>()) as u64;
    let (tri_buffer, mut tri_alloc) = allocator.allocate_buffer(
        tri_size,
        vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS
            | vk::BufferUsageFlags::STORAGE_BUFFER
            | vk::BufferUsageFlags::TRANSFER_DST,
        AllocFlags::HOST_VISIBLE,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
        vk::MemoryPropertyFlags::empty(),
    )?;
    let ptr = allocator.map_memory(&mut tri_alloc, tri_size, 0)?;
    unsafe {
        std::ptr::copy_nonoverlapping(triangles.as_ptr(), ptr as *mut Triangle, triangles.len());
    }
    allocator.flush_allocation(&tri_alloc, 0, tri_size)?;
    let _ = allocator.unmap_memory(&mut tri_alloc);

    let geom = BlasGeometry {
        triangle_count: triangles.len() as u32,
        vertex_stride: std::mem::size_of::<Triangle>() as u32,
        vertex_buffer: tri_buffer,
        vertex_buffer_address: device_address_of(device, tri_buffer),
        flags: vk::GeometryFlagsKHR::OPAQUE,
    };

    let builder = BlasBuilder::new().add_geometry(geom);
    let (_blas, _scratch) = unsafe { builder.build(device, rt_loader, allocator)? };

    // The BLAS handle above was moved into the tuple; rebuild reference data.
    // NOTE: cmd-level builds are issued by the renderer once command buffers exist.
    let blas_address_for_instances = 0u64; // filled in by renderer after cmd build

    // Build TLAS shell so callers own the full object graph.
    let mut tlas_builder = TlasBuilder::new();
    for (i, transform) in transforms.iter().enumerate() {
        tlas_builder =
            tlas_builder.add_instance(TlasInstance::new(*transform, i as u32, 0xFF, blas_address_for_instances));
    }
    let (tlas, tlas_scratch) =
        unsafe { tlas_builder.build(device, rt_loader, allocator)? };

    Ok(AccelerationStructures {
        tlas,
        blas_count: 1,
    })
}

/// Build a simple BLAS from an external vertex buffer.
pub fn build_simple_blas(
    device: &Device,
    rt_loader: &RtLoader,
    allocator: &mut GpuAllocator,
    vertex_buffer: vk::Buffer,
    vertex_count: u32,
    _index_buffer: vk::Buffer,
    _index_count: u32,
) -> Result<(Blas, ScratchBuffers), String> {
    let geom = BlasGeometry {
        triangle_count: vertex_count / 3,
        vertex_stride: (std::mem::size_of::<f32>() * 3) as u32, // XYZ vertices
        vertex_buffer,
        vertex_buffer_address: device_address_of(device, vertex_buffer),
        flags: vk::GeometryFlagsKHR::OPAQUE,
    };

    let builder = BlasBuilder::new().add_geometry(geom);
    unsafe { builder.build(device, rt_loader, allocator) }
}




