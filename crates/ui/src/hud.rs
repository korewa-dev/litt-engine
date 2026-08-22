//! Debug HUD -- FPS counter, render stats, NPU status.
//! Renders engine telemetry on screen.

use litt_math::Vec2;

/// HUD element to display
#[derive(Clone, Debug)]
pub struct HudElement {
    pub text: String,
    pub position: Vec2,
    pub color: [f32; 4],
    pub font_size: f32,
}

impl HudElement {
    pub fn new(text: &str, x: f32, y: f32, color: [f32; 4], font_size: f32) -> Self {
        Self {
            text: text.to_string(),
            position: Vec2(x, y),
            color,
            font_size,
        }
    }
}

/// Debug HUD
#[derive(Debug, Default)]
pub struct DebugHud {
    pub enabled: bool,
    pub elements: Vec<HudElement>,
    pub fps: f32,
    pub frame_time_ms: f32,
    pub draw_calls: u32,
    pub triangles: u32,
    pub npu_active: bool,
    pub npu_latency_ms: f32,
    /// Path trace samples accumulated so far
    pub path_trace_samples: u32,
    /// Whether path tracing is active
    pub path_trace_active: bool,
}

impl DebugHud {
    pub fn new() -> Self { Self::default() }

    pub fn update_stats(
        &mut self,
        fps: f32,
        frame_time: f32,
        draw_calls: u32,
        triangles: u32,
        npu_latency: f32,
    ) {
        self.fps = fps;
        self.frame_time_ms = frame_time;
        self.draw_calls = draw_calls;
        self.triangles = triangles;
        self.npu_latency_ms = npu_latency;
        self.npu_active = npu_latency > 0.0;
    }

    pub fn render_elements(&self) -> Vec<HudElement> {
        let mut elements = Vec::new();
        if !self.enabled { return elements; }

        let y_start = 10.0;
        let x = 10.0;
        let mut y = y_start;

        elements.push(HudElement::new(&format!("FPS: {:.1}", self.fps), x, y, [1.0, 1.0, 1.0, 1.0], 14.0));
        y += 18.0;
        elements.push(HudElement::new(&format!("Frame: {:.2}ms", self.frame_time_ms), x, y, [1.0, 1.0, 1.0, 1.0], 14.0));
        y += 18.0;
        elements.push(HudElement::new(&format!("Draw calls: {}", self.draw_calls), x, y, [1.0, 1.0, 1.0, 1.0], 14.0));
        y += 18.0;
        elements.push(HudElement::new(&format!("Triangles: {}", self.triangles), x, y, [1.0, 1.0, 1.0, 1.0], 14.0));
        y += 18.0;
        if self.npu_active {
            elements.push(HudElement::new(&format!("NPU: {:.1}ms", self.npu_latency_ms), x, y, [0.0, 1.0, 0.0, 1.0], 14.0));
            y += 18.0;
        }
        if self.path_trace_active {
            elements.push(HudElement::new(&format!("Path Tracer: {} samples", self.path_trace_samples), x, y, [1.0, 0.6, 0.2, 1.0], 14.0));
            y += 18.0;
        }
        elements.push(HudElement::new("Litt Engine", x, y, [0.5, 0.5, 0.5, 1.0], 12.0));

        elements
    }
}
