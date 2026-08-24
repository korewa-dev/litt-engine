//! GPU-compatible path tracing logic.
//! Translates Rust scene data to GPU buffers with full BLAS/TLAS support.

use ash::{vk, Device};
use litt_math::*;
use crate::scene::{Camera, Scene, Sphere, Light, SceneBounds, MaterialEntry};
use litt_vulkan::{
    AccelerationStructures, GpuAllocator, AllocFlags, RtLoader,
    build_acceleration_structures,
};

/// A GPU buffer plus its allocation bookkeeping.
pub struct Buffer {
    pub handle: vk::Buffer,
    pub memory: vk::DeviceMemory,
    pub size: u64,
    pub allocation: Option<litt_vulkan::Allocation>,
}

impl std::fmt::Debug for Buffer {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Buffer")
            .field("size", &self.size)
            .field("has_allocation", &self.allocation.is_some())
            .finish()
    }
}

/// A GPU image plus its allocation bookkeeping.
pub struct Image {
    pub handle: vk::Image,
    pub memory: vk::DeviceMemory,
    pub view: vk::ImageView,
    pub format: vk::Format,
    pub extent: [u32; 3],
    pub allocation: Option<litt_vulkan::Allocation>,
}

impl std::fmt::Debug for Image {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Image")
            .field("format", &(self.format.as_raw()))
            .field("extent", &self.extent)
            .finish()
    }
}

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
    pub scratch_buffer: Buffer,
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

/// Convert scene-space triangles into the tightly packed GPU layout.
pub fn to_gpu_triangles(scene: &Scene) -> Vec<litt_vulkan::Triangle> {
    scene
        .triangles
        .iter()
        .map(|t| litt_vulkan::Triangle {
            v0: [t.v0.0, t.v0.1, t.v0.2],
            v1: [t.v1.0, t.v1.1, t.v1.2],
            v2: [t.v2.0, t.v2.1, t.v2.2],
            normal: [t.normal.0, t.normal.1, t.normal.2],
            material_id: t.material_id,
            _pad: [0; 3],
        })
        .collect()
}

/// Build complete acceleration structure hierarchy from scene
pub fn build_scene_acceleration(
    device: &Device,
    rt_loader: &RtLoader,
    allocator: &mut GpuAllocator,
    scene: &Scene,
) -> Result<AccelerationStructures, String> {
    if scene.triangles.is_empty() {
        return Err("Cannot build acceleration structure with no triangles".to_string());
    }

    let gpu_triangles = to_gpu_triangles(scene);
    // One transform (identity) -- the whole scene is a single BLAS instance.
    let transforms = vec![[
        1.0f32, 0.0, 0.0, 0.0, //
        0.0, 1.0, 0.0, 0.0, //
        0.0, 0.0, 1.0, 0.0,
    ]];

    build_acceleration_structures(device, rt_loader, allocator, &gpu_triangles, &transforms)
}

/// Allocate a host-visible staging-backed storage buffer and fill it.
fn upload_slice<T: Copy>(
    allocator: &mut GpuAllocator,
    usage: vk::BufferUsageFlags,
    data: &[T],
) -> Result<(vk::Buffer, litt_vulkan::Allocation), String> {
    let size = (data.len() * std::mem::size_of::<T>()).max(std::mem::size_of::<T>()) as u64;
    let (buffer, mut alloc) = allocator.allocate_buffer(
        size,
        usage,
        AllocFlags(AllocFlags::HOST_VISIBLE.0 | AllocFlags::MAPPED.0),
        vk::MemoryPropertyFlags::HOST_VISIBLE,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;
    if !alloc.mapped.is_null() {
        unsafe {
            std::ptr::copy_nonoverlapping(data.as_ptr(), alloc.mapped as *mut T, data.len());
        }
    }
    allocator.flush_allocation(&alloc, 0, size)?;
    Ok((buffer, alloc))
}

/// Upload scene data to GPU buffers
///
/// `internal` is the render-scale resolution (FSR input size). The
/// accumulation/velocity/output images are allocated at exactly this size so
/// the path-trace dispatch, push constants and FSR input extents all agree.
pub fn upload_scene(
    _device: &Device,
    scene: &Scene,
    allocator: &mut GpuAllocator,
    internal: (u32, u32),
) -> Result<PathTracerBuffers, String> {
    // Triangles (packed GPU layout)
    let gpu_triangles = to_gpu_triangles(scene);
    let tri_usage =
        vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS;
    let (tri_buffer, tri_alloc) = upload_slice(allocator, tri_usage, &gpu_triangles)?;
    let tri_size = (gpu_triangles.len() * std::mem::size_of::<litt_vulkan::Triangle>()) as u64;

    // Spheres / lights / materials / bounds
    let sphere_data: Vec<Sphere> = scene.spheres.to_vec();
    let (sphere_buffer, sphere_alloc) =
        upload_slice(allocator, vk::BufferUsageFlags::STORAGE_BUFFER, &sphere_data)?;
    let sphere_size =
        (sphere_data.len().max(1) * std::mem::size_of::<Sphere>()) as u64;

    let light_data: Vec<Light> = scene.lights.to_vec();
    let (light_buffer, light_alloc) =
        upload_slice(allocator, vk::BufferUsageFlags::STORAGE_BUFFER, &light_data)?;
    let light_size = (light_data.len().max(1) * std::mem::size_of::<Light>()) as u64;

    let mat_data: Vec<MaterialEntry> = scene.materials.to_vec();
    let (mat_buffer, mat_alloc) =
        upload_slice(allocator, vk::BufferUsageFlags::STORAGE_BUFFER, &mat_data)?;
    let mat_size =
        (mat_data.len().max(1) * std::mem::size_of::<MaterialEntry>()) as u64;

    let bounds = [scene.bounds];
    let (bounds_buffer, bounds_alloc) =
        upload_slice(allocator, vk::BufferUsageFlags::STORAGE_BUFFER, &bounds)?;
    let bounds_size = std::mem::size_of::<SceneBounds>() as u64;

    // Scratch buffer for ray tracing (16 MB, device local)
    let scratch_size = 1024 * 1024 * 16u64;
    let (scratch_buffer, scratch_alloc) = allocator.allocate_buffer(
        scratch_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    // Accumulation buffer (device local) -- HDR
    let iw = internal.0.max(1);
    let ih = internal.1.max(1);
    let (accum_image, accum_view, accum_alloc) = allocator.allocate_image(
        [iw, ih, 1],
        vk::Format::R32G32B32A32_SFLOAT,
        vk::ImageUsageFlags::STORAGE
            | vk::ImageUsageFlags::TRANSFER_SRC
            | vk::ImageUsageFlags::TRANSFER_DST,
        AllocFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    // Velocity buffer -- motion vectors for reprojection
    let (velocity_image, velocity_view, vel_alloc) = allocator.allocate_image(
        [iw, ih, 1],
        vk::Format::R16G16_SFLOAT,
        vk::ImageUsageFlags::STORAGE,
        AllocFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    // Output buffer -- final display image
    let (output_image, output_view, out_alloc) = allocator.allocate_image(
        [iw, ih, 1],
        vk::Format::R8G8B8A8_UNORM,
        vk::ImageUsageFlags::STORAGE | vk::ImageUsageFlags::TRANSFER_SRC,
        AllocFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    Ok(PathTracerBuffers {
        scene_triangles: Buffer {
            handle: tri_buffer,
            memory: vk::DeviceMemory::null(),
            size: tri_size,
            allocation: Some(tri_alloc),
        },
        scene_spheres: Buffer {
            handle: sphere_buffer,
            memory: vk::DeviceMemory::null(),
            size: sphere_size,
            allocation: Some(sphere_alloc),
        },
        scene_lights: Buffer {
            handle: light_buffer,
            memory: vk::DeviceMemory::null(),
            size: light_size,
            allocation: Some(light_alloc),
        },
        scene_materials: Buffer {
            handle: mat_buffer,
            memory: vk::DeviceMemory::null(),
            size: mat_size,
            allocation: Some(mat_alloc),
        },
        scene_bounds: Buffer {
            handle: bounds_buffer,
            memory: vk::DeviceMemory::null(),
            size: bounds_size,
            allocation: Some(bounds_alloc),
        },
        accumulation_buffer: Image {
            handle: accum_image,
            memory: vk::DeviceMemory::null(),
            view: accum_view,
            format: vk::Format::R32G32B32A32_SFLOAT,
            extent: [iw, ih, 1],
            allocation: Some(accum_alloc),
        },
        velocity_buffer: Image {
            handle: velocity_image,
            memory: vk::DeviceMemory::null(),
            view: velocity_view,
            format: vk::Format::R16G16_SFLOAT,
            extent: [iw, ih, 1],
            allocation: Some(vel_alloc),
        },
        output_buffer: Image {
            handle: output_image,
            memory: vk::DeviceMemory::null(),
            view: output_view,
            format: vk::Format::R8G8B8A8_UNORM,
            extent: [iw, ih, 1],
            allocation: Some(out_alloc),
        },
        scratch_buffer: Buffer {
            handle: scratch_buffer,
            memory: vk::DeviceMemory::null(),
            size: scratch_size,
            allocation: Some(scratch_alloc),
        },
    })
}







