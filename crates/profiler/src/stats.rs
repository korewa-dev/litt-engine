//! Performance stats -- aggregate metrics for debugging.

#[derive(Debug, Default)]
pub struct Stats {
    /// Frame timing
    pub fps: f32,
    pub frame_time_ms: f32,
    pub cpu_time_ms: f32,
    pub gpu_time_ms: f32,

    /// Rendering
    pub draw_calls: u32,
    pub triangles: u32,
    pub instanced_draws: u32,
    pub texture_binds: u32,
    pub shader_switches: u32,

    /// Memory
    pub gpu_memory_mb: f32,
    pub vertex_memory_mb: f32,
    pub index_memory_mb: f32,
    pub texture_memory_mb: f32,
    pub buffer_memory_mb: f32,

    /// AI/NPU
    pub npu_inferences: u32,
    pub npu_latency_ms: f32,
    pub ai_memory_mb: f32,

    /// Physics
    pub physics_bodies: u32,
    pub collisions: u32,
    pub physics_time_ms: f32,
}

impl Stats {
    pub fn new() -> Self { Self::default() }

    /// Reset frame stats
    pub fn reset_frame(&mut self) {
        self.draw_calls = 0;
        self.triangles = 0;
        self.instanced_draws = 0;
        self.texture_binds = 0;
        self.shader_switches = 0;
        self.npu_inferences = 0;
        self.collisions = 0;
        self.npu_latency_ms = 0.0;
        self.physics_time_ms = 0.0;
    }
}
