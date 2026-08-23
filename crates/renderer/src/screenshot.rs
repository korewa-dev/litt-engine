//! Screenshot capture -- GPU image to CPU bytes readback.
//!
//! Copies a swapchain/render-target image into a host-visible staging
//! buffer and returns tightly-packed RGBA8 rows ready to save.
//!
//! ```ignore
//! let rgba = capture_image_rgba(&device, &pool, queue, &mut allocator,
//!                                swapchain_image, width, height)?;
//! write_ppm("shot.ppm", &rgba, width, height)?;
//! ```

use ash::{vk, Device};
use litt_vulkan::{GpuAllocator, AllocFlags};

use crate::command_pool::CommandPool;

/// Bytes per pixel for supported capture formats.
fn bytes_per_pixel(format: vk::Format) -> Result<u32, String> {
    match format {
        vk::Format::R8G8B8A8_UNORM | vk::Format::B8G8R8A8_UNORM => Ok(4),
        _ => Err(format!("Unsupported capture format (raw vk::Format {})", format.as_raw())),
    }
}

/// Swap B<->R in place (BGRA <-> RGBA).
pub fn bgra_to_rgba(pixels: &mut [u8]) {
    for px in pixels.chunks_exact_mut(4) {
        px.swap(0, 2);
    }
}

/// Copy `image` into a host-visible buffer and return tightly packed rows.
///
/// Layout transitions are handled internally: pass the layout the image is
/// currently in; after capture it is restored so caller pipeline state survives.
///
/// Row padding from the driver is stripped; output stride is exactly
/// `width * bpp`.
pub fn capture_image(
    device: &Device,
    pool: &CommandPool,
    queue: vk::Queue,
    allocator: &mut GpuAllocator,
    image: vk::Image,
    current_layout: vk::ImageLayout,
    width: u32,
    height: u32,
    format: vk::Format,
) -> Result<Vec<u8>, String> {
    let bpp = bytes_per_pixel(format)?;
    let row_size = (width as usize) * bpp as usize;
    let buf_size = (row_size * height as usize).max(16);

    // Host-visible staging buffer (auto-mapped by HOST_VISIBLE flag)
    let (buffer, mut allocation) = allocator.allocate_buffer(
        buf_size as u64,
        vk::BufferUsageFlags::TRANSFER_DST,
        AllocFlags::HOST_VISIBLE,
        vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
        vk::MemoryPropertyFlags::HOST_VISIBLE,
    )?;

    let result = (|| -> Result<Vec<u8>, String> {
        let cmd = pool.begin_single_time_commands()?;

        // Transition image to TRANSFER_SRC_OPTIMAL
        pool.transition_image_layout(
            cmd,
            image,
            vk::ImageAspectFlags::COLOR,
            current_layout,
            vk::ImageLayout::TRANSFER_SRC_OPTIMAL,
            vk::PipelineStageFlags::ALL_COMMANDS,
            vk::PipelineStageFlags::TRANSFER,
            vk::AccessFlags::MEMORY_WRITE,
            vk::AccessFlags::TRANSFER_READ,
        )?;

        let region = vk::BufferImageCopy {
            buffer_offset: 0,
            buffer_row_length: 0, // tightly packed
            buffer_image_height: 0,
            image_subresource: vk::ImageSubresourceLayers {
                aspect_mask: vk::ImageAspectFlags::COLOR,
                mip_level: 0,
                base_array_layer: 0,
                layer_count: 1,
            },
            image_offset: vk::Offset3D { x: 0, y: 0, z: 0 },
            image_extent: vk::Extent3D { width, height, depth: 1 },
        };

        unsafe {
            device.cmd_copy_image_to_buffer(
                cmd,
                image,
                vk::ImageLayout::TRANSFER_SRC_OPTIMAL,
                buffer,
                &[region],
            );
        }

        // Transition back so the caller's pipeline state survives captures
        pool.transition_image_layout(
            cmd,
            image,
            vk::ImageAspectFlags::COLOR,
            vk::ImageLayout::TRANSFER_SRC_OPTIMAL,
            current_layout,
            vk::PipelineStageFlags::TRANSFER,
            vk::PipelineStageFlags::ALL_COMMANDS,
            vk::AccessFlags::TRANSFER_READ,
            vk::AccessFlags::MEMORY_READ | vk::AccessFlags::MEMORY_WRITE,
        )?;

        pool.end_single_time_commands(cmd, queue)?;

        // Read back through the persistent mapping
        let ptr = allocator.map_memory(&mut allocation, buf_size as u64, 0)?;
        let mut out = vec![0u8; buf_size];
        unsafe {
            std::ptr::copy_nonoverlapping(ptr as *const u8, out.as_mut_ptr(), buf_size);
        }
        allocator.unmap_memory(&mut allocation)?;

        // Convert to RGBA if needed
        if format == vk::Format::B8G8R8A8_UNORM {
            bgra_to_rgba(&mut out);
        }

        Ok(out)
    })();

    allocator.free_buffer(buffer, &mut allocation);
    result
}

/// Convenience wrapper capturing a B8G8R8A8_UNORM image as RGBA8.
pub fn capture_image_rgba(
    device: &Device,
    pool: &CommandPool,
    queue: vk::Queue,
    allocator: &mut GpuAllocator,
    image: vk::Image,
    current_layout: vk::ImageLayout,
    width: u32,
    height: u32,
) -> Result<Vec<u8>, String> {
    capture_image(
        device, pool, queue, allocator, image, current_layout,
        width, height, vk::Format::B8G8R8A8_UNORM, // swapchain-common; converted internally
    )
}

/// Write raw RGBA8 pixels as a binary PPM (P6, RGB). Zero dependencies,
/// viewable by nearly every image tool.
pub fn write_ppm(path: &str, rgba: &[u8], width: u32, height: u32) -> Result<(), String> {
    let expected = width as usize * height as usize * 4;
    if rgba.len() < expected {
        return Err(format!("write_ppm: need {} bytes, got {}", expected, rgba.len()));
    }

    use std::io::Write;
    let file = std::fs::File::create(path)
        .map_err(|e| format!("create '{}' failed: {}", path, e))?;
    let mut w = std::io::BufWriter::new(file);

    write!(w, "P6\n{} {}\n255\n", width, height)
        .map_err(|e| format!("ppm header failed: {}", e))?;

    let mut rgb = Vec::with_capacity(expected / 4 * 3);
    for px in rgba[..expected].chunks_exact(4) {
        rgb.extend_from_slice(&[px[0], px[1], px[2]]);
    }
    w.write_all(&rgb).map_err(|e| format!("ppm body failed: {}", e))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn ppm_writes_header_and_pixels() {
        let dir = std::env::temp_dir().join("litt_shot_test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("t.ppm");
        let path = path.to_str().unwrap();

        // 2x2 red/green/blue/white
        let px = [255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255];
        super::write_ppm(path, &px, 2, 2).unwrap();

        let data = std::fs::read(path).unwrap();
        assert_eq!(&data[..3], b"P6\n");
        assert_eq!(data.len(), "P6\n2 2\n255\n".len() + 12);
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn swizzle_roundtrip() {
        let mut px = [1u8, 2, 3, 4];
        super::bgra_to_rgba(&mut px);
        assert_eq!(px, [3, 2, 1, 4]);
        super::bgra_to_rgba(&mut px);
        assert_eq!(px, [1, 2, 3, 4]);
    }
}
