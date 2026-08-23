//! Complete path tracing backend with BLAS/TLAS and FSR 3.1.5 integration.
//! Full GPU buffer management via the litt-vulkan GpuAllocator.
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

/// Default camera for the path tracer demo scene
pub fn default_camera() -> Camera {
    Camera {
        position: Vec3::new(0.0, 2.0, 8.0),
        rotation: Vec2::new(0.0, 0.0),
        fov: 90.0,
        aspect: 16.0 / 9.0,
        ..Default::default()
    }
}

/// Default path tracer scene -- a room with an emissive light sphere
pub fn default_scene() -> Scene {
    Scene::default_test_scene()
}
use litt_math::{Vec2, Vec3};
pub use crate::scene::Camera;


