//! Performance report -- generates a text report of performance data.
//! Used for logging and debugging.

use super::frame_timer::FrameTimer;
use super::bottleneck::BottleneckAnalyzer;
use super::fps_history::FpsHistory;
use super::memory_profiler::GpuMemoryStats;

/// Performance report
#[derive(Debug)]
pub struct PerfReport {
    pub title: String,
    pub lines: Vec<String>,
}

impl Default for PerfReport {
    fn default() -> Self { Self::new() }
}

impl PerfReport {
    pub fn new() -> Self {
        Self {
            title: "Performance Report".to_string(),
            lines: Vec::new(),
        }
    }

    /// Generate a full performance report
    pub fn generate(
        timer: &FrameTimer,
        bottleneck: &BottleneckAnalyzer,
        fps_history: &FpsHistory,
        gpu_memory: &GpuMemoryStats,
        stats: &super::stats::Stats,
    ) -> Self {
        let mut report = Self::new();

        report.title = format!("Litt Engine Performance Report -- {}", timer.frame_count);

        report.lines.push("=".repeat(60));
        report.lines.push(report.title.clone());
        report.lines.push("=".repeat(60));
        report.lines.push(String::new());

        // Frame timing
        report.lines.push("FRAME TIMING".to_string());
        report.lines.push("-".repeat(40));
        report.lines.push(format!("  Current FPS:     {:.1}", timer.fps));
        report.lines.push(format!("  Last frame:      {:.2}ms", timer.last_frame_ms));
        report.lines.push(format!("  Average:         {:.2}ms", timer.avg_frame_ms));
        report.lines.push(format!("  Min frame:       {:.2}ms", timer.min_frame_ms));
        report.lines.push(format!("  Max frame:       {:.2}ms", timer.max_frame_ms));
        report.lines.push(String::new());

        // FPS history
        report.lines.push("FPS HISTORY".to_string());
        report.lines.push("-".repeat(40));
        let fps_stats = fps_history.stats();
        report.lines.push(format!("  Average:  {:.1} fps", fps_stats.avg));
        report.lines.push(format!("  Min:      {:.1} fps", fps_stats.min));
        report.lines.push(format!("  Max:      {:.1} fps", fps_stats.max));
        report.lines.push(format!("  1% Low:   {:.1} fps", fps_stats.one_percent_low));
        report.lines.push(format!("  Stutter:  {:.2} fps stddev", fps_stats.stutter));
        report.lines.push(format!("  Quality:  {}", fps_stats.quality()));
        report.lines.push(String::new());

        // Bottleneck analysis
        report.lines.push("BOTTLENECK ANALYSIS".to_string());
        report.lines.push("-".repeat(40));
        let bn = bottleneck.bottleneck();
        report.lines.push(format!("  Type:       {}", bn.type_));
        report.lines.push(format!("  Time:       {:.2}ms ({:.1}%)", bn.time_ms, bn.percent));
        report.lines.push(format!("  Fix:        {}", bn.recommendation));
        report.lines.push(String::new());

        // Stage breakdown
        report.lines.push("STAGE BREAKDOWN".to_string());
        report.lines.push("-".repeat(40));
        for (name, pct) in bottleneck.timing.percentages() {
            let bar_len = (pct / 10.0) as usize;
            let bar = "#".repeat(bar_len);
            report.lines.push(format!("  {name:12} {pct:>6.1}%  {bar}"));
        }
        report.lines.push(String::new());

        // Memory
        report.lines.push("MEMORY".to_string());
        report.lines.push("-".repeat(40));
        report.lines.push(format!("  GPU: {}", gpu_memory.report()));
        report.lines.push(format!("  GPU Peak: {:.1} MB", gpu_memory.peak_mb()));
        report.lines.push(String::new());

        // Render stats
        report.lines.push("RENDER".to_string());
        report.lines.push("-".repeat(40));
        report.lines.push(format!("  Draw calls:     {}", stats.draw_calls));
        report.lines.push(format!("  Triangles:      {}", stats.triangles));
        report.lines.push(format!("  Instanced draws:{}", stats.instanced_draws));
        report.lines.push(format!("  Texture binds:  {}", stats.texture_binds));
        report.lines.push(format!("  Shader switches:{}", stats.shader_switches));
        report.lines.push(String::new());

        // AI/NPU
        report.lines.push("AI/NPU".to_string());
        report.lines.push("-".repeat(40));
        report.lines.push(format!("  Inferences: {}", stats.npu_inferences));
        report.lines.push(format!("  Latency:    {:.2}ms", stats.npu_latency_ms));
        report.lines.push(format!("  Memory:     {:.1} MB", stats.ai_memory_mb));
        report.lines.push(String::new());

        // Physics
        report.lines.push("PHYSICS".to_string());
        report.lines.push("-".repeat(40));
        report.lines.push(format!("  Bodies:     {}", stats.physics_bodies));
        report.lines.push(format!("  Collisions: {}", stats.collisions));
        report.lines.push(format!("  Time:       {:.2}ms", stats.physics_time_ms));
        report.lines.push(String::new());

        report
    }

    /// Get the report as a string
    pub fn to_string(&self) -> String {
        self.lines.join("\n")
    }

    /// Save report to file
    pub fn save(&self, path: &str) -> Result<(), String> {
        std::fs::write(path, self.to_string())
            .map_err(|e| format!("Failed to save report: {e}"))?;
        Ok(())
    }
}
