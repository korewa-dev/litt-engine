//! Complete path tracing backend with BLAS/TLAS and FSR 3.1.5 integration.
//! Full GPU buffer management with VMA memory allocator.
//! Includes ReSTIR for efficient light sampling.

pub mod scene;
pub mod tracer;
pub mod material;
pub mod rng;
pub mod restir;
pub mod camera_controls;

pub use scene::*;
pub use tracer::*;
pub use material::*;
pub use rng::*;
pub use restir::*;
pub use camera_controls::*;

use ash::{vk, Device};
use crate::vulkan::{VmaAllocator, AccelerationStructures, BlasBuilder, TlasBuilder, TlasInstance, BlasGeometry, AllocFlags};

/// Default camera for the path tracer demo scene
pub fn default_camera() -> Camera {
    Camera {
        position: Vec3::new(0.0, 2.0, 8.0),
        rotation: Vec2::new(0.0, 0.0),
        fov: 90.0,
        aspect: 16.0 / 9.0,
    }
}

/// Default path tracer scene -- a room with an emissive light sphere
pub fn default_scene() -> Scene {
    Scene::default_test_scene()
}

/// Build complete BLAS + TLAS hierarchy from scene
pub fn build_scene_acceleration(
    device: &Device,
    rt_loader: &ash::extensions::khr::RayTracingPipeline,
    allocator: &mut VmaAllocator,
    scene: &Scene,
) -> Result<AccelerationStructures, String> {
    if scene.triangles.is_empty() {
        return Err("Cannot build acceleration structure with no triangles".to_string());
    }

    // Build BLAS from all triangles
    let mut blas_builder = BlasBuilder::new();
    
    // Create buffer with all triangle data
    let tri_size = (scene.triangles.len() * std::mem::size_of::<Triangle>()) as u64;
    let (tri_buffer, tri_alloc) = allocator.allocate_buffer(
        tri_size,
        vk::BufferUsageFlags::STORAGE_BUFFER | vk::BufferUsageFlags::SHADER_DEVICE_ADDRESS,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    // Upload triangle data
    let ptr = allocator.map_memory(&tri_alloc, tri_size, 0)?;
    unsafe {
        for i in 0..scene.triangles.len() {
            std::ptr::write(ptr.add(i * std::mem::size_of::<Triangle>()) as *mut Triangle, scene.triangles[i]);
        }
    }
    allocator.flush_allocation(&tri_alloc, 0, tri_size)?;

    let geom = BlasGeometry {
        triangle_count: scene.triangles.len() as u32,
        vertex_stride: std::mem::size_of::<Triangle>() as u32,
        index_buffer: tri_buffer,
        flags: vk::AccelerationStructureGeometryFlagsKHR::OPAQUE,
    };

    blas_builder = blas_builder.add_geometry(geom);

    let blas = unsafe { blas_builder.build(device, rt_loader, allocator)? };

    // Build TLAS with single instance
    let mut tlas_builder = TlasBuilder::new();
    let instance = TlasInstance {
        transform: [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
        ],
        instance_custom_index: 0,
        mask: 0xFF,
        instance_geometry_offset: 0,
        acceleration_structure_reference: blas.device_address,
    };
    tlas_builder = tlas_builder.add_instance(instance);

    let (tlas, _scratch) = unsafe { tlas_builder.build(device, rt_loader, allocator, &[blas.handle])? };

    Ok(AccelerationStructures {
        tlas,
        blas_count: 1,
    })
}

/// Upload complete scene to GPU buffers
pub fn upload_scene(
    device: &Device,
    scene: &Scene,
    allocator: &mut VmaAllocator,
) -> Result<PathTracerBuffers, String> {
    // Upload triangles
    let tri_size = (scene.triangles.len() * std::mem::size_of::<Triangle>()) as u64;
    let (tri_buffer, tri_alloc) = allocator.allocate_buffer(
        tri_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    let ptr = allocator.map_memory(&tri_alloc, tri_size, 0)?;
    unsafe {
        for i in 0..scene.triangles.len() {
            std::ptr::write(ptr.add(i * std::mem::size_of::<Triangle>()) as *mut Triangle, scene.triangles[i]);
        }
    }
    allocator.flush_allocation(&tri_alloc, 0, tri_size)?;

    // Upload spheres
    let sphere_size = (scene.spheres.len() * std::mem::size_of::<Sphere>()) as u64;
    let (sphere_buffer, sphere_alloc) = allocator.allocate_buffer(
        sphere_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    let ptr = allocator.map_memory(&sphere_alloc, sphere_size, 0)?;
    unsafe {
        for i in 0..scene.spheres.len() {
            std::ptr::write(ptr.add(i * std::mem::size_of::<Sphere>()) as *mut Sphere, scene.spheres[i]);
        }
    }
    allocator.flush_allocation(&sphere_alloc, 0, sphere_size)?;

    // Upload lights
    let light_size = (scene.lights.len() * std::mem::size_of::<Light>()) as u64;
    let (light_buffer, light_alloc) = allocator.allocate_buffer(
        light_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    let ptr = allocator.map_memory(&light_alloc, light_size, 0)?;
    unsafe {
        for i in 0..scene.lights.len() {
            std::ptr::write(ptr.add(i * std::mem::size_of::<Light>()) as *mut Light, scene.lights[i]);
        }
    }
    allocator.flush_allocation(&light_alloc, 0, light_size)?;

    // Upload materials
    let mat_size = (scene.materials.len() * std::mem::size_of::<MaterialEntry>()) as u64;
    let (mat_buffer, mat_alloc) = allocator.allocate_buffer(
        mat_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    let ptr = allocator.map_memory(&mat_alloc, mat_size, 0)?;
    unsafe {
        for i in 0..scene.materials.len() {
            std::ptr::write(ptr.add(i * std::mem::size_of::<MaterialEntry>()) as *mut MaterialEntry, scene.materials[i]);
        }
    }
    allocator.flush_allocation(&mat_alloc, 0, mat_size)?;

    // Scene bounds
    let bounds_size = std::mem::size_of::<SceneBounds>() as u64;
    let (bounds_buffer, bounds_alloc) = allocator.allocate_buffer(
        bounds_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::HOST_VISIBLE | AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    let ptr = allocator.map_memory(&bounds_alloc, bounds_size, 0)?;
    unsafe {
        std::ptr::write(ptr as *mut SceneBounds, scene.bounds);
    }
    allocator.flush_allocation(&bounds_alloc, 0, bounds_size)?;

    // Scratch buffer for ray tracing
    let scratch_size = 1024 * 1024 * 16; // 16MB scratch
    let (scratch_buffer, scratch_alloc) = allocator.allocate_buffer(
        scratch_size,
        vk::BufferUsageFlags::STORAGE_BUFFER,
        AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
    )?;

    // Create accumulation buffer (device local)
    let (accum_image, _accum_view, accum_alloc) = allocator.allocate_image(
        [640, 360, 1],
        vk::Format::R32G32B32A32_SFLOAT,
        vk::ImageUsageFlags::STORAGE_IMAGE | vk::ImageUsageFlags::TRANSFER_SRC | vk::ImageUsageFlags::TRANSFER_DST,
        AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    let (velocity_image, _vel_view, vel_alloc) = allocator.allocate_image(
        [640, 360, 1],
        vk::Format::R16G16_SFLOAT,
        vk::ImageUsageFlags::STORAGE_IMAGE,
        AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    let (output_image, _out_view, out_alloc) = allocator.allocate_image(
        [640, 360, 1],
        vk::Format::R8G8B8A8_UNORM,
        vk::ImageUsageFlags::STORAGE_IMAGE | vk::ImageUsageFlags::TRANSFER_SRC,
        AllocFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        vk::MemoryPropertyFlags::DEVICE_LOCAL,
        1,
        1,
    )?;

    Ok(PathTracerBuffers {
        scene_triangles: Buffer {
            handle: tri_buffer,
            memory: vk::DeviceMemory::null(),
            size: tri_size,
            allocation: None,
        },
        scene_spheres: Buffer {
            handle: sphere_buffer,
            memory: vk::DeviceMemory::null(),
            size: sphere_size,
            allocation: None,
        },
        scene_lights: Buffer {
            handle: light_buffer,
            memory: vk::DeviceMemory::null(),
            size: light_size,
            allocation: None,
        },
        scene_materials: Buffer {
            handle: mat_buffer,
            memory: vk::DeviceMemory::null(),
            size: mat_size,
            allocation: None,
        },
        scene_bounds: Buffer {
            handle: bounds_buffer,
            memory: vk::DeviceMemory::null(),
            size: bounds_size,
            allocation: None,
        },
        accumulation_buffer: Image {
            handle: accum_image,
            memory: vk::DeviceMemory::null(),
            view: vk::ImageView::null(),
            format: vk::Format::R32G32B32A32_SFLOAT,
            extent: [640, 360, 1],
            allocation: None,
        },
        velocity_buffer: Image {
            handle: velocity_image,
            memory: vk::DeviceMemory::null(),
            view: vk::ImageView::null(),
            format: vk::Format::R16G16_SFLOAT,
            extent: [640, 360, 1],
            allocation: None,
        },
        output_buffer: Image {
            handle: output_image,
            memory: vk::DeviceMemory::null(),
            view: vk::ImageView::null(),
            format: vk::Format::R8G8B8A8_UNORM,
            extent: [640, 360, 1],
            allocation: None,
        },
        scratch_buffer: Buffer {
            handle: scratch_buffer,
            memory: vk::DeviceMemory::null(),
            size: scratch_size,
            allocation: None,
        },
    })
}
