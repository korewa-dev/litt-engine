//! Logical resource descriptors -- the backend-neutral "shape" of GPU
//! resources. Every backend materializes these into native objects
//! (VkBuffer + VMA allocation, ID3D12Resource on a heap, ...).

// ---------------------------------------------------------------------------
// Tiny bitflags macro (avoids pulling the bitflags dependency into GAL).
// Must be declared before first use in this file.
// ---------------------------------------------------------------------------

macro_rules! bitflags_lite {
    (
        $(#[$outer:meta])*
        pub struct $name:ident : $repr:ty {
            $($(#[$fmeta:meta])* const $flag:ident = $val:expr;)+
        }
    ) => {
        $(#[$outer])*
        #[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
        pub struct $name($repr);

        impl $name {
            $($(#[$fmeta])* pub const $flag: $name = $name($val);)+

            /// All flags ORed together.
            pub const ALL: $name = $name(0 $(| $val)+);

            /// Raw representation (push constants, logs).
            #[inline]
            pub const fn bits(self) -> $repr { self.0 }

            #[inline]
            pub const fn contains(self, other: $name) -> bool {
                (self.0 & other.0) == other.0
            }

            #[inline]
            pub const fn union(self, other: $name) -> $name {
                $name(self.0 | other.0)
            }

            #[inline]
            pub const fn is_empty(self) -> bool {
                self.0 == 0
            }
        }

        impl std::ops::BitOr for $name {
            type Output = $name;
            #[inline]
            fn bitor(self, rhs: $name) -> $name { $name(self.0 | rhs.0) }
        }

        impl std::ops::BitOrAssign for $name {
            #[inline]
            fn bitor_assign(&mut self, rhs: $name) { self.0 |= rhs.0; }
        }
    };
}

/// Where a buffer's memory lives.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MemoryLocation {
    /// GPU-only, fastest access (VMA DEVICE_LOCAL / D3D12 DEFAULT heap).
    DeviceLocal,
    /// CPU-writes-GPU-reads upload path (VMA HOST_VISIBLE / UPLOAD heap).
    HostToGpu,
    /// GPU-writes-CPU-reads readback path (READBACK heap).
    GpuToHost,
}

bitflags_lite! {
    /// What a buffer may be used for.
    pub struct BufferUsage: u32 {
        const NONE = 0;
        const VERTEX = 1 << 0;
        const INDEX = 1 << 1;
        const UNIFORM = 1 << 2;
        const STORAGE = 1 << 3;
        const INDIRECT = 1 << 4;
        const TRANSFER_SRC = 1 << 5;
        const TRANSFER_DST = 1 << 6;
        const ACCELERATION_STRUCTURE = 1 << 7;
    }
}

bitflags_lite! {
    /// What an image may be used for.
    pub struct ImageUsage: u32 {
        const NONE = 0;
        const SAMPLED = 1 << 0;
        const STORAGE = 1 << 1;
        const COLOR_TARGET = 1 << 2;
        const DEPTH_TARGET = 1 << 3;
        const TRANSFER_SRC = 1 << 4;
        const TRANSFER_DST = 1 << 5;
    }
}

/// Pixel format subset shared by every backend. Backends map these to
/// VkFormat / DXGI_FORMAT internally; unmappable formats are rejected at
/// creation time with `GalError::InvalidDescriptor`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Format {
    R8Unorm,
    Rg8Unorm,
    Rgba8Unorm,
    Bgra8Unorm,
    R16Float,
    Rgba16Float,
    R32Float,
    Rg32Float,
    Rgba32Float,
    D32Float,
    D24UnormS8Uint,
    Bc7Rgba,
}

impl Format {
    /// Bytes per pixel for uncompressed formats (BC formats report block size).
    pub const fn bytes_per_pixel(self) -> u32 {
        match self {
            Format::R8Unorm => 1,
            Format::Rg8Unorm => 2,
            Format::Rgba8Unorm | Format::Bgra8Unorm => 4,
            Format::R16Float => 2,
            Format::Rgba16Float => 8,
            Format::R32Float => 4,
            Format::Rg32Float => 8,
            Format::Rgba32Float => 16,
            Format::D32Float => 4,
            Format::D24UnormS8Uint => 4,
            // BC7: 16 bytes per 4x4 block -> 1 byte/px effective
            Format::Bc7Rgba => 1,
        }
    }
}

/// Logical buffer descriptor.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct BufferDesc {
    /// Size in bytes.
    pub size: u64,
    /// Allowed usage bits.
    pub usage: BufferUsage,
    /// Memory placement.
    pub location: MemoryLocation,
}

impl BufferDesc {
    /// Storage buffer living in device memory (SSBO / UAV).
    pub const fn storage(size: u64) -> Self {
        Self { size, usage: BufferUsage::STORAGE.union(BufferUsage::TRANSFER_DST), location: MemoryLocation::DeviceLocal }
    }

    /// Uniform block, host-visible for per-frame constant updates.
    pub const fn uniform(size: u64) -> Self {
        Self { size, usage: BufferUsage::UNIFORM, location: MemoryLocation::HostToGpu }
    }

    /// Vertex buffer.
    pub const fn vertex(size: u64) -> Self {
        Self { size, usage: BufferUsage::VERTEX.union(BufferUsage::TRANSFER_DST), location: MemoryLocation::DeviceLocal }
    }

    /// Index buffer.
    pub const fn index(size: u64) -> Self {
        Self { size, usage: BufferUsage::INDEX.union(BufferUsage::TRANSFER_DST), location: MemoryLocation::DeviceLocal }
    }
}

/// Logical image descriptor.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ImageDesc {
    pub width: u32,
    pub height: u32,
    /// 1 for 2D textures.
    pub depth: u32,
    pub format: Format,
    pub usage: ImageUsage,
    pub location: MemoryLocation,
}

impl ImageDesc {
    /// Standard LDR RGBA render target.
    pub const fn color_target(width: u32, height: u32) -> Self {
        Self {
            width,
            height,
            depth: 1,
            format: Format::Bgra8Unorm,
            usage: ImageUsage::COLOR_TARGET.union(ImageUsage::SAMPLED).union(ImageUsage::TRANSFER_SRC),
            location: MemoryLocation::DeviceLocal,
        }
    }

    /// HDR accumulation target for the path tracer.
    pub const fn hdr_target(width: u32, height: u32) -> Self {
        Self {
            width,
            height,
            depth: 1,
            format: Format::Rgba32Float,
            usage: ImageUsage::COLOR_TARGET.union(ImageUsage::STORAGE).union(ImageUsage::TRANSFER_SRC),
            location: MemoryLocation::DeviceLocal,
        }
    }
}

/// Which shader stage a pipeline entry point belongs to.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
    RayGen,
    ClosestHit,
    Miss,
}

/// Logical pipeline descriptor. Shader payloads are SPIR-V words; DX12
/// backends translate via DXC or accept DXIL through the `dxil` field.
#[derive(Clone, Debug, Default)]
pub struct PipelineDesc {
    /// Stage name -> SPIR-V words (empty for external pipelines).
    pub spir_v: Vec<(ShaderStage, Vec<u8>)>,
    /// Pre-translated DXIL blobs (optional; DX12 backend prefers these).
    pub dxil: Vec<(ShaderStage, Vec<u8>)>,
    /// Push constant / root constant budget in bytes.
    pub push_constant_size: u32,
    /// Number of storage buffers bound at slots 0..n.
    pub buffer_slots: u32,
    /// Number of sampled images bound at slots 0..n.
    pub image_slots: u32,
    /// Human-readable debug label ("pbr-opaque", "pathtrace-main").
    pub label: String,
}

/// Swapchain description for presentation-capable backends.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SwapchainDesc {
    pub width: u32,
    pub height: u32,
    /// 0 = let the driver decide (FIFO always available everywhere).
    pub image_count: u32,
    /// Enable tearing if supported (for frame generation).
    pub allow_tearing: bool,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn usage_flags_compose() {
        let u = BufferUsage::VERTEX.union(BufferUsage::TRANSFER_DST);
        assert!(u.contains(BufferUsage::VERTEX));
        assert!(u.contains(BufferUsage::TRANSFER_DST));
        assert!(!u.contains(BufferUsage::INDEX));
    }

    #[test]
    fn desc_constructors() {
        let b = BufferDesc::storage(4096);
        assert_eq!(b.size, 4096);
        assert!(b.usage.contains(BufferUsage::STORAGE));
        let i = ImageDesc::color_target(1920, 1080);
        assert_eq!(i.format.bytes_per_pixel(), 4);
    }
}

