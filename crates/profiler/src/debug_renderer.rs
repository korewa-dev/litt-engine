//! Debug renderer — renders debug overlays like wireframes, normals, and AABBs.
//! Provides GPU-compatible debug primitives for visualization.

use litt_math::{Vec3, Vec2, Mat4};

/// Debug primitive type
#[derive(Clone, Debug)]
pub enum DebugPrimitive {
    /// Bounding box (8 corners + edges)
    BoundingBox {
        min: Vec3,
        max: Vec3,
        color: [f32; 4],
    },
    /// Wireframe sphere
    WireSphere {
        center: Vec3,
        radius: f32,
        color: [f32; 4],
        segments: u32,
    },
    /// Normal vector
    Normal {
        origin: Vec3,
        direction: Vec3,
        color: [f32; 4],
        length: f32,
    },
    /// Velocity vector
    Velocity {
        origin: Vec3,
        velocity: Vec3,
        color: [f32; 4],
    },
    /// Ray
    Ray {
        origin: Vec3,
        direction: Vec3,
        length: f32,
        color: [f32; 4],
    },
    /// Text label
    Text {
        text: String,
        position: Vec3,
        color: [f32; 4],
    },
    /// Grid
    Grid {
        center: Vec3,
        size: f32,
        divisions: u32,
        color: [f32; 4],
    },
}

/// Debug renderer
#[derive(Debug, Default)]
pub struct DebugRenderer {
    pub primitives: Vec<DebugPrimitive>,
    pub enabled: bool,
    pub show_wireframe: bool,
    pub show_normals: bool,
    pub show_bounds: bool,
    pub show_velocities: bool,
}

impl DebugRenderer {
    pub fn new() -> Self { Self::default() }

    pub fn draw_box(&mut self, min: Vec3, max: Vec3, color: [f32; 4]) {
        self.primitives.push(DebugPrimitive::BoundingBox { min, max, color });
    }

    pub fn draw_sphere(&mut self, center: Vec3, radius: f32, color: [f32; 4], segments: u32) {
        self.primitives.push(DebugPrimitive::WireSphere {
            center, radius, color, segments,
        });
    }

    pub fn draw_normal(&mut self, origin: Vec3, direction: Vec3, color: [f32; 4], length: f32) {
        self.primitives.push(DebugPrimitive::Normal { origin, direction, color, length });
    }

    pub fn draw_velocity(&mut self, origin: Vec3, velocity: Vec3, color: [f32; 4]) {
        self.primitives.push(DebugPrimitive::Velocity { origin, velocity, color });
    }

    pub fn draw_ray(&mut self, origin: Vec3, direction: Vec3, length: f32, color: [f32; 4]) {
        self.primitives.push(DebugPrimitive::Ray { origin, direction, length, color });
    }

    pub fn draw_text(&mut self, text: &str, position: Vec3, color: [f32; 4]) {
        self.primitives.push(DebugPrimitive::Text {
            text: text.to_string(),
            position,
            color,
        });
    }

    pub fn draw_grid(&mut self, center: Vec3, size: f32, divisions: u32, color: [f32; 4]) {
        self.primitives.push(DebugPrimitive::Grid { center, size, divisions, color });
    }

    pub fn clear(&mut self) {
        self.primitives.clear();
    }

    pub fn is_empty(&self) -> bool {
        self.primitives.is_empty()
    }

    /// Get the count of each primitive type
    pub fn counts(&self) -> PrimitiveCounts {
        let mut counts = PrimitiveCounts::default();
        for prim in &self.primitives {
            match prim {
                DebugPrimitive::BoundingBox { .. } => counts.bounding_boxes += 1,
                DebugPrimitive::WireSphere { .. } => counts.spheres += 1,
                DebugPrimitive::Normal { .. } => counts.normals += 1,
                DebugPrimitive::Velocity { .. } => counts.velocities += 1,
                DebugPrimitive::Ray { .. } => counts.rays += 1,
                DebugPrimitive::Text { .. } => counts.texts += 1,
                DebugPrimitive::Grid { .. } => counts.grids += 1,
            }
        }
        counts
    }
}

/// Primitive counts
#[derive(Debug, Default)]
pub struct PrimitiveCounts {
    pub bounding_boxes: u32,
    pub spheres: u32,
    pub normals: u32,
    pub velocities: u32,
    pub rays: u32,
    pub texts: u32,
    pub grids: u32,
}

/// Debug overlay configuration
#[derive(Clone, Debug)]
pub struct DebugOverlayConfig {
    pub enabled: bool,
    pub show_wireframe: bool,
    pub show_normals: bool,
    pub show_bounds: bool,
    pub show_velocities: bool,
    pub show_physics: bool,
    pub show_ray_tracing: bool,
    pub show_gpu_profile: bool,
}

impl Default for DebugOverlayConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            show_wireframe: false,
            show_normals: false,
            show_bounds: true,
            show_velocities: false,
            show_physics: false,
            show_ray_tracing: false,
            show_gpu_profile: false,
        }
    }
}
