//! Physics backend selector -- GPU compute vs CPU fallback
//!
//! Selects the optimal physics backend based on available hardware:
//! - **GPU**: Vulkan compute shaders for RDNA/AMD, MUSA for Moore Threads
//! - **CPU**: BVH broadphase + SAT narrowphase with platform-specific SIMD

/// Available physics backends
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PhysicsBackend {
    /// GPU compute (Vulkan compute shaders) -- RDNA, Intel Arc, Moore Threads
    GPU,
    /// CPU fallback with platform-specific SIMD (AVX2/NEON/RVV)
    CPU,
    /// Auto-detect best available backend
    Auto,
}

impl Default for PhysicsBackend {
    fn default() -> Self { Self::Auto }
}

impl PhysicsBackend {
    pub fn is_gpu(&self) -> bool { matches!(self, Self::GPU | Self::Auto) }
    pub fn is_cpu(&self) -> bool { matches!(self, Self::CPU) }

    /// Detect the best available backend for the current platform
    pub fn detect() -> Self {
        #[cfg(target_arch = "x86_64")]
        {
            // Check for Vulkan compute support
            Self::Auto
        }
        #[cfg(target_arch = "aarch64")]
        {
            // ARM with NEON support
            Self::Auto
        }
        #[cfg(target_arch = "riscv64")]
        {
            // RISC-V with RVV support
            Self::CPU
        }
        #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64", target_arch = "riscv64")))]
        {
            Self::CPU
        }
    }

    /// Get a human-readable name for the backend
    pub fn name(&self) -> &'static str {
        match self {
            Self::GPU => "GPU (Vulkan Compute)",
            Self::CPU => "CPU (SIMD Optimized)",
            Self::Auto => "Auto (Platform Optimal)",
        }
    }
}

/// GPU compute kernel data for physics
#[derive(Clone, Debug)]
pub struct PhysicsComputeData {
    pub body_buffer: Vec<u8>,
    pub transform_buffer: Vec<u8>,
    pub body_count: u32,
    pub cell_size: f32,
}

impl PhysicsComputeData {
    pub fn new(body_count: u32, cell_size: f32) -> Self {
        Self { body_buffer: Vec::new(), transform_buffer: Vec::new(), body_count, cell_size }
    }

    pub fn reserve(&mut self, n: u32) {
        self.body_buffer.reserve(n as usize * 128);
        self.transform_buffer.reserve(n as usize * 48);
    }
}
