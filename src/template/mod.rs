//! Template components for Litt Engine
//!
//! These components bridge the gap between the ECS world and the graphics pipeline.
//! Physics components are now in the litt_physics crate.

#[path = "components/transform.rs"]
pub mod transform;
#[path = "components/camera.rs"]
pub mod camera;
#[path = "components/player.rs"]
pub mod player;
#[path = "components/mesh.rs"]
pub mod mesh;
#[path = "components/material.rs"]
pub mod material;
#[path = "components/light.rs"]
pub mod light;

pub use transform::*;
pub use camera::*;
pub use player::*;
pub use mesh::*;
pub use material::*;
pub use light::*;

// Re-export as components for legacy paths
pub mod components {
    pub use super::{camera::*, light::*, material::*, mesh::*, player::*, transform::*};
}


