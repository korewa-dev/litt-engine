//! Bottleneck analysis -- identifies which system is limiting performance.
//! Compares CPU, GPU, NPU, and Physics timings to find the slowest stage.

use super::frame_timing::FrameTimingBreakdown;

/// Performance bottleneck analysis
#[derive(Debug)]
pub struct BottleneckAnalyzer {
    pub timing: FrameTimingBreakdown,
    pub cpu_time_ms: f32,
    pub gpu_time_ms: f32,
    pub npu_time_ms: f32,
    pub physics_time_ms: f32,
    pub frame_time_ms: f32,
}

impl Default for BottleneckAnalyzer {
    fn default() -> Self { Self::new() }
}

impl BottleneckAnalyzer {
    pub fn new() -> Self {
        Self {
            timing: FrameTimingBreakdown::new(),
            cpu_time_ms: 0.0,
            gpu_time_ms: 0.0,
            npu_time_ms: 0.0,
            physics_time_ms: 0.0,
            frame_time_ms: 0.0,
        }
    }

    /// Update with current frame data
    pub fn update(&mut self, cpu_ms: f32, gpu_ms: f32, npu_ms: f32, physics_ms: f32, frame_ms: f32) {
        self.cpu_time_ms = cpu_ms;
        self.gpu_time_ms = gpu_ms;
        self.npu_time_ms = npu_ms;
        self.physics_time_ms = physics_ms;
        self.frame_time_ms = frame_ms;
    }

    /// Get the current bottleneck
    pub fn bottleneck(&self) -> BottleneckInfo {
        let mut max_time = 0.0f32;
        let mut max_type = "None".to_string();
        let mut max_percent = 0.0f32;

        if self.cpu_time_ms > max_time {
            max_time = self.cpu_time_ms;
            max_type = "CPU".to_string();
            max_percent = self.cpu_time_ms / self.frame_time_ms.max(0.001) * 100.0;
        }
        if self.gpu_time_ms > max_time {
            max_time = self.gpu_time_ms;
            max_type = "GPU".to_string();
            max_percent = self.gpu_time_ms / self.frame_time_ms.max(0.001) * 100.0;
        }
        if self.npu_time_ms > max_time {
            max_time = self.npu_time_ms;
            max_type = "NPU".to_string();
            max_percent = self.npu_time_ms / self.frame_time_ms.max(0.001) * 100.0;
        }
        if self.physics_time_ms > max_time {
            max_time = self.physics_time_ms;
            max_type = "Physics".to_string();
            max_percent = self.physics_time_ms / self.frame_time_ms.max(0.001) * 100.0;
        }

        let recommendation = self.recommendation(&max_type);
        BottleneckInfo {
            type_: max_type,
            time_ms: max_time,
            percent: max_percent,
            recommendation,
        }
    }

    /// Get recommendation for the current bottleneck
    fn recommendation(&self, bottleneck: &str) -> String {
        match bottleneck {
            "CPU" => "Reduce CPU overhead: batch draw calls, reduce draw call count, use instancing".to_string(),
            "GPU" => "GPU bound: lower resolution, reduce draw calls, optimize shaders, use FSR".to_string(),
            "NPU" => "NPU inference slow: reduce model complexity, use fewer NPCs, lower inference frequency".to_string(),
            "Physics" => "Physics bound: reduce body count, use simpler collision shapes, lower physics frequency".to_string(),
            _ => "No significant bottleneck detected".to_string(),
        }
    }

    /// Get a performance summary
    pub fn summary(&self) -> String {
        let bn = self.bottleneck();
        format!(
            "Frame: {:.2}ms | CPU: {:.2}ms ({:.0}%) | GPU: {:.2}ms ({:.0}%) | NPU: {:.2}ms | Physics: {:.2}ms | Bottleneck: {} ({:.1}%)",
            self.frame_time_ms,
            self.cpu_time_ms, self.cpu_time_ms / self.frame_time_ms.max(0.001) * 100.0,
            self.gpu_time_ms, self.gpu_time_ms / self.frame_time_ms.max(0.001) * 100.0,
            self.npu_time_ms,
            self.physics_time_ms,
            bn.type_, bn.percent
        )
    }
}

/// Bottleneck information
#[derive(Debug)]
pub struct BottleneckInfo {
    pub type_: String,
    pub time_ms: f32,
    pub percent: f32,
    pub recommendation: String,
}
