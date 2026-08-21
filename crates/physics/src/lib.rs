//! GPU-accelerated rigid body physics system for Litt Engine
//!
//! Implements the multi-tier physics pipeline described in docs/physics/physics-system.md:
//! - **RDNA/AMD**: GPU compute via Vulkan compute shaders (WGSL/GLSL)
//! - **ARM/Mobile**: NEON-intrinsics-accelerated CPU fallback
//! - **RISC-V**: RVV vectorized CPU fallback
//! - **Default**: Spatial hash broadphase + SAT narrowphase CPU path
//!
//! The system runs on a separate compute queue for async execution on RDNA hardware.

#![allow(clippy::missing_safety_intrinsic)]
#![allow(clippy::type_complexity)]

pub mod physics_body;
pub mod broadphase;
pub mod narrowphase;
pub mod integrator;
pub mod system;
pub mod backend;

pub use physics_body::*;
pub use broadphase::*;
pub use narrowphase::*;
pub use integrator::*;
pub use system::*;
pub use backend::*;
