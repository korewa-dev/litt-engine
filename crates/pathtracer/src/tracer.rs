//! GPU-compatible path tracing logic.
//! Translates Rust scene data to GPU buffers.

use ash::{vk, Device};
use litt_math::*;
use super::*;

/// Path tracing results pushed back to CPU for accumulation
#[derive(Debug)]
pub struct TraceResult {
    pub color: Vec3,
    pub throughput: f32,
    pub bounce: u32,
}

/// GPU buffer bindings for the path tracer
#[derive(Debug)]
pub struct PathTracerBuffers {
    pub scene_triangles: Buffer,
    pub scene_spheres: Buffer,
    pub scene_lights: Buffer,
    pub scene_materials: Buffer,
    pub scene_bounds: Buffer,
    pub accumulation_buffer: Image,
    pub velocity_buffer: Image,
    pub output_buffer: Image,
}

/// Constants for the path tracer push constant buffer
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PathTracerConstants {
    pub resolution_x: u32,
    pub resolution_y: u32,
    pub max_bounces: u32,
    pub frame_count: u32,
    pub camera_pos_x: f32,
    pub camera_pos_y: f32,
    pub camera_pos_z: f32,
    pub camera_yaw: f32,
    pub camera_pitch: f32,
    pub fov: f32,
    pub aspect: f32,
    pub light_count: u32,
    pub _pad: [u32; 3],
}

impl PathTracerConstants {
    pub fn new(width: u32, height: u32, camera: &Camera, scene: &Scene) -> Self {
        Self {
            resolution_x: width,
            resolution_y: height,
            max_bounces: 8,
            frame_count: 0,
            camera_pos_x: camera.position.0,
            camera_pos_y: camera.position.1,
            camera_pos_z: camera.position.2,
            camera_yaw: camera.rotation.0,
            camera_pitch: camera.rotation.1,
            fov: camera.fov,
            aspect: camera.aspect,
            light_count: scene.lights.len() as u32,
            _pad: [0; 3],
        }
    }
}

/// Build acceleration structure from scene triangles
pub fn build_blas_from_triangles(
    device: &Device,
    triangles: &[Triangle],
) -> Result<AccelerationStructure, String> {
    // For the minimal implementation, we store triangle data in a buffer
    // and use the buffer as the input to the ray tracing shader
    // Full AS build would require VK_KHR_acceleration_structure

    // Create a buffer with triangle data
    let tri_data: Vec<u8> = triangles.iter()
        .flat_map(|t| {
            let mut bytes = Vec::with_capacity(64);
            bytes.extend_from_slice(bytemuck::bytes_of(&t.v0));
            bytes.extend_from_slice(bytemuck::bytes_of(&t.v1));
            bytes.extend_from_slice(bytemuck::bytes_of(&t.v2));
            bytes.extend_from_slice(bytemuck::bytes_of(&t.normal));
            bytes.extend_from_slice(&t.material_id.to_le_bytes());
            bytes.resize(bytes.len() + (64 - bytes.len() % 64), 0);
            bytes
        })
        .collect();

    let buffer_info = vk::BufferCreateInfo::builder()
        .size(tri_data.len() as u64)
        .usage(vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS | vk::BufferUsageFlags::STORAGE_BUFFER)
        .sharing_mode(vk::SharingMode::EXCLUSIVE)
        .build();

    let buffer = unsafe { device.create_buffer(&buffer_info, None)
        .map_err(|e| format!("Triangle buffer creation failed: {:?}", e))? };

    // For simplicity, return a placeholder AS
    // In production, build proper BLAS using rt_device.build_acceleration_structures_khr
    Ok(AccelerationStructure {
        handle: vk::AccelerationStructureKHR::null(),
        memory: vk::DeviceMemory::null(),
        size: tri_data.len() as u64,
        allocation: None,
    })
}

/// Upload triangle data to GPU buffer
pub fn upload_triangles(
    device: &Device,
    triangles: &[Triangle],
) -> Result<Buffer, String> {
    let size = (triangles.len() * std::mem::size_of::<Triangle>()) as u64;
    let info = vk::BufferCreateInfo::builder()
        .size(size)
        .usage(vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::TRANSFER_DST)
        .sharing_mode(vk::SharingMode::EXCLUSIVE)
        .build();

    let buffer = unsafe { device.create_buffer(&info, None)
        .map_err(|e| format!("Buffer creation failed: {:?}", e))? };

    // Map and copy
    unsafe {
        let ptr = device.map_memory(vk::DeviceMemory::null(), 0, size, vk::MemoryMapFlags::empty())
            .map_err(|_| "Map failed")?;
        std::ptr::write_bytes(ptr as *mut Triangle, Triangle::default(), triangles.len());
        device.unmap_memory();
    }

    Ok(Buffer {
        handle: buffer,
        memory: vk::DeviceMemory::null(),
        size,
        allocation: None,
    })
}
