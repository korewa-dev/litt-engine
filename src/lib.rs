//! Litt Engine Library
//! Re-exports all engine crates and modules for the GUI and tests.

pub use litt_math::*;
pub use litt_platform::*;
#[cfg(feature = "vulkan")]
pub use litt_vulkan::*;
pub use litt_renderer::*;
pub use litt_pathtracer::*;
pub use litt_fidelityfx::*;
pub use litt_physics::*;
pub use litt_ai::*;
pub use litt_asset::*;
pub use litt_input::*;
pub use litt_audio::*;
pub use litt_ui::*;
pub use litt_profiler::*;
pub use litt_scene::*;
pub use litt_config::*;

// Graphics backend abstraction
pub mod graphics;
pub use graphics::{GraphicsBackend, select_backend, get_gpu_info};

// Headless editor core (interactive shell lives in the binary)
pub mod editor;

// SceneGraph -> pathtracer Scene deployment bridge
pub mod world_bridge;

// Native gameplay session (the ported runtime contract)
pub mod gameplay;

// Deterministic replay recording/playback
pub mod replay;

// Re-export config crate under a different name to avoid conflict
pub use litt_config as config;
