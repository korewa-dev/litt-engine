//! Litt Engine Library
//! Re-exports all engine crates and modules for the GUI and tests.

pub use litt_math::*;
pub use litt_platform::*;
#[cfg(feature = "vulkan")]
pub use litt_vulkan::*;
pub use litt_renderer::*;
pub use litt_pathtracer::*;
pub use litt_fidelityfx::*;
#[cfg(feature = "dx12")]
pub use litt_dx12::*;
pub use litt_physics::*;
pub use litt_ai::*;
pub use litt_asset::*;
pub use litt_input::*;
pub use litt_audio::*;
pub use litt_ui::*;
pub use litt_profiler::*;
pub use litt_scene::*;
pub use litt_config::*;
pub use litt_net::*;

// Graphics backend abstraction
pub mod graphics;
pub use graphics::{GraphicsBackend, select_backend, get_gpu_info};

// Deterministic replay recording/playback
pub mod replay;

// Re-export config crate under a different name to avoid conflict
pub use litt_config as config;
