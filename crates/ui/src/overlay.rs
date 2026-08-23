//! Overlay renderer -- renders debug overlays on top of the scene.
//! Bounding boxes, gizmos, wireframes.

use litt_math::{Vec3, Vec2, Mat4};

/// Overlay primitive
#[derive(Clone, Debug)]
pub enum OverlayPrimitive {
    Line { start: Vec3, end: Vec3, color: [f32; 4] },
    Sphere { center: Vec3, radius: f32, color: [f32; 4] },
    Box { min: Vec3, max: Vec3, color: [f32; 4] },
    Point { position: Vec3, size: f32, color: [f32; 4] },
    Text { text: String, position: Vec2, color: [f32; 4], font_size: f32 },
}

/// Overlay renderer
#[derive(Debug, Default)]
pub struct Overlay {
    pub primitives: Vec<OverlayPrimitive>,
    pub enabled: bool,
}

impl Overlay {
    pub fn new() -> Self { Self::default() }

    pub fn draw_line(&mut self, start: Vec3, end: Vec3, color: [f32; 4]) {
        self.primitives.push(OverlayPrimitive::Line { start, end, color });
    }

    pub fn draw_box(&mut self, min: Vec3, max: Vec3, color: [f32; 4]) {
        self.primitives.push(OverlayPrimitive::Box { min, max, color });
    }

    pub fn draw_sphere(&mut self, center: Vec3, radius: f32, color: [f32; 4]) {
        self.primitives.push(OverlayPrimitive::Sphere { center, radius, color });
    }

    pub fn draw_text(&mut self, text: &str, x: f32, y: f32, color: [f32; 4], font_size: f32) {
        self.primitives.push(OverlayPrimitive::Text {
            text: text.to_string(),
            position: Vec2(x, y),
            color,
            font_size,
        });
    }

    pub fn clear(&mut self) {
        self.primitives.clear();
    }

    pub fn is_empty(&self) -> bool {
        self.primitives.is_empty()
    }
}
