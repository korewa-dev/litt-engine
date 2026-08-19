//! Ray tracing pipeline and acceleration structure management.
//! Uses VK_KHR_ray_tracing_pipeline and VK_KHR_acceleration_structure.

use ash::{vk, Device, extensions::khr};
use bytemuck::{Pod, Zeroable};
use super::*;

/// Ray tracing scene with BLAS + TLAS
#[derive(Debug)]
pub struct RayTracingScene {
    pub tlas: AccelerationStructure,
    pub blas_count: u32,
}

/// Ray tracing pipeline handle
#[derive(Debug)]
pub struct RayTracingPipelineHandle {
    pub pipeline: RayTracingPipeline,
    pub descriptor_set_layout: vk::DescriptorSetLayout,
    pub pipeline_layout: vk::PipelineLayout,
    pub max_ray_recursion_depth: u32,
}

// =============================================================================
// Ray Tracing Geometry
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct RtGeometry {
    pub vertices: [*const f32; 3],  // v0, v1, v2
    pub normals: [*const f32; 3],
    pub triangle_count: u32,
    pub vertex_stride: u32,
    pub index_buffer: vk::Buffer,
    pub index_offset: u32,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct RtTriangle {
    pub v0: Vec3,
    pub v1: Vec3,
    pub v2: Vec3,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct RtInstance {
    pub transform: [f32; 12],  // 3x4 column-major
    pub instance_mask: u32,
    pub instance_id: u32,
    pub sbt_index_offset: u32,
    pub flags: u32,
}

// =============================================================================
// SBT (Shader Binding Table)
// =============================================================================

#[derive(Debug)]
pub struct ShaderBindingTable {
    pub raygen_record: SbtRecord,
    pub miss_records: Vec<SbtRecord>,
    pub hit_group_records: Vec<SbtRecord>,
    pub callable_records: Vec<SbtRecord>,
}

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct SbtRecord {
    pub shader_group_handle: u64,
    pub continuation_data: u64,
}

// =============================================================================
// Build acceleration structures
// =============================================================================

/// Build a BLAS (Bottom-Level Acceleration Structure)
pub fn build_blas(
    device: &Device,
    rt_device: &khr::RayTracingPipeline,
    geometries: &[RtGeometry],
    scratch_buffer: &vk::Buffer,
    scratch_offset: u64,
) -> Result<AccelerationStructure, String> {
    let mut geometries_vk: Vec<vk::AccelerationStructureGeometryKHR> = Vec::new();
    let mut geometries_data: Vec<vk::AccelerationStructureGeometryDataKHR> = Vec::new();

    for geom in geometries {
        let tri = vk::AccelerationStructureGeometryTrianglesDataKHR::builder()
            .device_buffer(geom.index_buffer)
            .device_offset(geom.index_offset)
            .triangle_count(geom.triangle_count)
            .vertex_data(vk::AccelStructGeometryDataKHR {
                vertices: vk::AccelerationStructureGeometryTrianglesDataKHR {
                    vertex_data: vk::AccelStructGeometryDataKHR {
                        buffer: vk::Buffer::null(), // Use vertex buffer instead
                        offset: 0,
                        stride: geom.vertex_stride,
                    },
                    ..Default::default()
                },
            })
            .build();

        let data = vk::AccelerationStructureGeometryDataKHR { triangles: tri };
        let geom_vk = vk::AccelerationStructureGeometryKHR::builder()
            .geometry_type(vk::AccelerationStructureGeometryTypeKHR::TRIANGLES)
            .geometry(data)
            .flags(vk::AccelerationStructureGeometryFlagsKHR::OPAQUE)
            .build();

        geometries_vk.push(geom_vk);
        geometries_data.push(data);
    }

    let info = vk::AccelerationStructureBuildGeometryInfoKHR::builder()
        .type_(vk::AccelerationStructureTypeKHR::BOTTOM_LEVEL)
        .geometries(&geometries_vk)
        .scratch_data(vk::AccelerationStructureDeviceAddressInfoKHR {
            acceleration_structure: vk::AccelerationStructureKHR::null(),
            device_address: scratch_buffer as u64 + scratch_offset,
        })
        .build();

    // For simplicity, return a placeholder
    // In production, use rt_device.build_acceleration_structures_khr
    Ok(AccelerationStructure {
        handle: vk::AccelerationStructureKHR::null(),
        memory: vk::DeviceMemory::null(),
        size: 0,
        allocation: None,
    })
}

/// Build a TLAS (Top-Level Acceleration Structure)
pub fn build_tlas(
    device: &Device,
    rt_device: &khr::RayTracingPipeline,
    blas_handles: &[vk::AccelerationStructureKHR],
    instances: &[RtInstance],
    scratch_buffer: &vk::Buffer,
    scratch_offset: u64,
) -> Result<AccelerationStructure, String> {
    // Create instance buffer
    let instance_size = (instances.len() * std::mem::size_of::<RtInstance>()) as u64;
    let instance_buffer_info = vk::BufferCreateInfo::builder()
        .size(instance_size)
        .usage(vk::BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS)
        .sharing_mode(vk::SharingMode::EXCLUSIVE)
        .build();

    let instance_buffer = unsafe { device.create_buffer(&instance_buffer_info, None)
        .map_err(|e| format!("Failed to create instance buffer: {:?}", e))? };

    // Map and fill
    let mut instance_data: Vec<RtInstance> = instances.to_vec();
    unsafe {
        let ptr = device.map_memory(
            vk::DeviceMemory::null(),
            0,
            instance_size,
            vk::MemoryMapFlags::empty(),
        ).map_err(|_| "Map failed")?;
        std::ptr::write_bytes(ptr as *mut RtInstance, RtInstance::default(), instances.len());
        device.unmap_memory();
    }

    // Build TLAS info
    let mut instances_vk: Vec<vk::AccelerationStructureInstanceKHR> = instances.iter().map(|inst| {
        vk::AccelerationStructureInstanceKHR {
            transform: vk::TransformMatrixKHR {
                matrix: inst.transform,
            },
            instance_mask: inst.instance_mask,
            instance_index: inst.instance_id,
            mask: inst.instance_mask,
            flags: vk::AccelerationStructureInstanceFlagsKHR::empty(),
            acceleration_structure_reference: vk::AccelerationStructureReferenceKHR {
                acceleration_structure: inst.instance_id as u64,
            },
        }
    }).collect();

    let geom = vk::AccelerationStructureGeometryKHR::builder()
        .geometry_type(vk::AccelerationStructureGeometryTypeKHR::INSTANCES)
        .geometry(vk::AccelerationStructureGeometryDataKHR {
            instances: vk::AccelerationStructureGeometryInstancesDataKHR::builder()
                .data(vk::AccelStructGeometryDataKHR {
                    buffer: instance_buffer,
                    offset: 0,
                    stride: std::mem::size_of::<vk::AccelerationStructureInstanceKHR>() as u32,
                })
                .build()
        })
        .build();

    let build_info = vk::AccelerationStructureBuildGeometryInfoKHR::builder()
        .type_(vk::AccelerationStructureTypeKHR::TOP_LEVEL)
        .flags(vk::BuildAccelerationStructureFlagsKHR::PREFER_FAST_BUILD)
        .geometries(&[geom])
        .scratch_data(vk::AccelerationStructureDeviceAddressInfoKHR {
            acceleration_structure: vk::AccelerationStructureKHR::null(),
            device_address: scratch_buffer as u64 + scratch_offset,
        })
        .build();

    // Query build sizes
    let sizes = unsafe {
        rt_device.get_acceleration_structure_build_sizes_khr(
            vk::AccelerationStructureBuildTypeKHR::DEVICE,
            &build_info,
            &[instances.len() as u32],
        )
    };

    // Create TLAS
    let tlas_info = vk::AccelerationStructureCreateInfoKHR::builder()
        .size(sizes.acceleration_structure_size)
        .build();

    // ... (full implementation would continue here)
    // For now return placeholder

    Ok(AccelerationStructure {
        handle: vk::AccelerationStructureKHR::null(),
        memory: vk::DeviceMemory::null(),
        size: sizes.acceleration_structure_size,
        allocation: None,
    })
}

// =============================================================================
// Ray Tracing Pipeline Creation
// =============================================================================

/// Create a ray tracing pipeline
pub fn create_ray_tracing_pipeline(
    device: &Device,
    rt_loader: &khr::RayTracingPipeline,
    raygen_spv: &[u32],
    miss_spv: &[u32],
    chit_spv: &[u32],
    max_recursion_depth: u32,
    descriptor_set_layout: vk::DescriptorSetLayout,
    push_constant_size: u32,
) -> Result<RayTracingPipeline, String> {
    use ash::vk::ShaderGroupShaderKHR;

    // Shader modules
    let raygen_module = create_shader_module(device, raygen_spv)?;
    let miss_module = create_shader_module(device, miss_spv)?;
    let chit_module = create_shader_module(device, chit_spv)?;

    // Shader stage creatives
    let raygen_info = vk::PipelineShaderStageCreateInfo::builder()
        .stage(vk::ShaderStageFlags::RAYGEN)
        .module(raygen_module)
        .p_name(std::ffi::CString::new("main").unwrap().as_ptr())
        .build();

    let miss_info = vk::PipelineShaderStageCreateInfo::builder()
        .stage(vk::ShaderStageFlags::MISS)
        .module(miss_module)
        .p_name(std::ffi::CString::new("main").unwrap().as_ptr())
        .build();

    let chit_info = vk::PipelineShaderStageCreateInfo::builder()
        .stage(vk::ShaderStageFlags::CLOSEST_HIT)
        .module(chit_module)
        .p_name(std::ffi::CString::new("main").unwrap().as_ptr())
        .build();

    // Shader groups
    let groups = vec![
        vk::RayTracingShaderGroupCreateInfoKHR::builder()
            .shader_group_handle(vk::ShaderGroupShaderKHR::RAYGEN)
            .generic_shader_group(false)
            .raygen_shader(vk::ShaderStageFlags::RAYGEN)
            .build(),
        vk::RayTracingShaderGroupCreateInfoKHR::builder()
            .shader_group_handle(vk::ShaderGroupShaderKHR::MISS)
            .generic_shader_group(false)
            .miss_shader(vk::ShaderStageFlags::MISS)
            .build(),
        vk::RayTracingShaderGroupCreateInfoKHR::builder()
            .shader_group_handle(vk::ShaderGroupShaderKHR::CLOSEST_HIT)
            .generic_shader_group(false)
            .closest_hit_shader(vk::ShaderStageFlags::CLOSEST_HIT)
            .build(),
    ];

    // Pipeline layout
    let layout_info = vk::PipelineLayoutCreateInfo::builder()
        .push_constant_ranges(&[vk::PushConstantRange {
            stage_flags: vk::ShaderStageFlags::RAYGEN,
            offset: 0,
            size: push_constant_size,
        }])
        .set_layouts(&[descriptor_set_layout])
        .build();
    let pipeline_layout = unsafe { device.create_pipeline_layout(&layout_info, None)
        .map_err(|e| format!("Pipeline layout creation failed: {:?}", e))? };

    // Pipeline
    let stage_creates = vec![raygen_info, miss_info, chit_info];
    let pipeline_info = vk::RayTracingPipelineCreateInfoKHR::builder()
        .stages(&stage_creates)
        .groups(&groups)
        .max_pipeline_ray_recursion_depth(max_recursion_depth)
        .layout(pipeline_layout)
        .build();

    let pipeline = unsafe {
        rt_loader.create_ray_tracing_pipelines_khr(
            vk::PipelineCache::null(),
            &[pipeline_info],
            None,
        ).map_err(|e| format!("RT pipeline creation failed: {:?}", e))?[0]
    };

    let group_handle_size = unsafe {
        rt_loader.get_ray_tracing_shader_group_handles_khr(
            pipeline, 0, 1024,
        ).map_err(|e| format!("Failed to get shader group handles: {:?}", e))?
    };

    // Cleanup
    unsafe {
        device.destroy_shader_module(raygen_module, None);
        device.destroy_shader_module(miss_module, None);
        device.destroy_shader_module(chit_module, None);
    }

    Ok(RayTracingPipeline {
        pipeline,
        pipeline_layout,
        shader_group_handle_size: group_handle_size,
    })
}

fn create_shader_module(device: &Device, code: &[u32]) -> Result<vk::ShaderModule, String> {
    let info = vk::ShaderModuleCreateInfo::builder().code(code).build();
    unsafe { device.create_shader_module(&info, None)
        .map_err(|e| format!("Shader module creation failed: {:?}", e)) }
}
