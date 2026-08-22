//! Frame timing breakdown -- measures time spent in each rendering stage.
//! Provides detailed per-stage timing for bottleneck analysis.

use std::time::Instant;

/// Rendering stage timing
#[derive(Debug, Clone, Default)]
pub struct StageTiming {
    pub name: &'static str,
    pub begin: Option<Instant>,
    pub elapsed_ms: f32,
    pub samples: Vec<f32>,
}

impl StageTiming {
    pub fn new(name: &'static str) -> Self {
        Self {
            name,
            begin: None,
            elapsed_ms: 0.0,
            samples: Vec::new(),
        }
    }

    pub fn start(&mut self) {
        self.begin = Some(Instant::now());
    }

    pub fn stop(&mut self) {
        if let Some(begin) = self.begin.take() {
            let elapsed = begin.elapsed().as_secs_f32() * 1000.0;
            self.elapsed_ms = elapsed;
            if self.samples.len() >= 60 {
                self.samples.remove(0);
            }
            self.samples.push(elapsed);
        }
    }

    pub fn avg(&self) -> f32 {
        if self.samples.is_empty() { return 0.0; }
        self.samples.iter().sum::<f32>() / self.samples.len() as f32
    }

    pub fn min(&self) -> f32 {
        self.samples.iter().cloned().fold(f32::MAX, f32::min)
    }

    pub fn max(&self) -> f32 {
        self.samples.iter().cloned().fold(0.0, f32::max)
    }
}

/// Frame timing stages
#[derive(Debug, Clone)]
pub enum TimingStage {
    BeginFrame,
    Input,
    Physics,
    AI,
    Culling,
    Upload,
    Draw,
    Present,
    EndFrame,
    Total,
}

impl TimingStage {
    pub fn name(&self) -> &'static str {
        match self {
            Self::BeginFrame => "BeginFrame",
            Self::Input => "Input",
            Self::Physics => "Physics",
            Self::AI => "AI",
            Self::Culling => "Culling",
            Self::Upload => "Upload",
            Self::Draw => "Draw",
            Self::Present => "Present",
            Self::EndFrame => "EndFrame",
            Self::Total => "Total",
        }
    }
}

/// Frame timing breakdown
#[derive(Debug)]
pub struct FrameTimingBreakdown {
    pub stages: [StageTiming; 10],
    pub total_ms: f32,
    pub bottleneck: Option<BottleneckType>,
}

impl Default for FrameTimingBreakdown {
    fn default() -> Self { Self::new() }
}

impl FrameTimingBreakdown {
    pub fn new() -> Self {
        Self {
            stages: [
                StageTiming::new("BeginFrame"),
                StageTiming::new("Input"),
                StageTiming::new("Physics"),
                StageTiming::new("AI"),
                StageTiming::new("Culling"),
                StageTiming::new("Upload"),
                StageTiming::new("Draw"),
                StageTiming::new("Present"),
                StageTiming::new("EndFrame"),
                StageTiming::new("Total"),
            ],
            total_ms: 0.0,
            bottleneck: None,
        }
    }

    /// Get a mutable reference to a stage
    pub fn stage(&mut self, stage: TimingStage) -> &mut StageTiming {
        &mut self.stages[stage as usize]
    }

    /// Get a reference to a stage
    pub fn get_stage(&self, stage: TimingStage) -> &StageTiming {
        &self.stages[stage as usize]
    }

    /// Record total frame time
    pub fn record_total(&mut self, ms: f32) {
        self.total_ms = ms;
        self.stages[TimingStage::Total as usize].elapsed_ms = ms;
        self.detect_bottleneck();
    }

    /// Detect which stage is the bottleneck
    pub fn detect_bottleneck(&mut self) {
        let mut max_stage = 0usize;
        let mut max_time = 0.0f32;

        for (i, stage) in self.stages.iter().enumerate() {
            if i == TimingStage::Total as usize { continue; }
            if stage.avg() > max_time {
                max_time = stage.avg();
                max_stage = i;
            }
        }

        self.bottleneck = match max_stage {
            0 => Some(BottleneckType::BeginFrame),
            1 => Some(BottleneckType::Input),
            2 => Some(BottleneckType::Physics),
            3 => Some(BottleneckType::AI),
            4 => Some(BottleneckType::Culling),
            5 => Some(BottleneckType::Upload),
            6 => Some(BottleneckType::Draw),
            7 => Some(BottleneckType::Present),
            8 => Some(BottleneckType::EndFrame),
            _ => None,
        };
    }

    /// Get bottleneck description
    pub fn bottleneck_desc(&self) -> String {
        match self.bottleneck {
            Some(BottleneckType::Physics) => format!("Physics ({:.2}ms)", self.stages[2].avg()),
            Some(BottleneckType::AI) => format!("AI/NPU ({:.2}ms)", self.stages[3].avg()),
            Some(BottleneckType::Draw) => format!("Draw calls ({:.2}ms)", self.stages[6].avg()),
            Some(BottleneckType::Upload) => format!("Upload ({:.2}ms)", self.stages[5].avg()),
            Some(BottleneckType::Culling) => format!("Culling ({:.2}ms)", self.stages[4].avg()),
            Some(BottleneckType::Present) => format!("Present ({:.2}ms)", self.stages[7].avg()),
            _ => "Unknown".to_string(),
        }
    }

    /// Get percentage of total time for each stage
    pub fn percentages(&self) -> Vec<(String, f32)> {
        if self.total_ms <= 0.0 { return Vec::new(); }
        self.stages.iter()
            .filter(|s| s.name != "Total")
            .map(|s| (s.name.to_string(), s.avg() / self.total_ms * 100.0))
            .collect()
    }
}

/// Bottleneck type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BottleneckType {
    BeginFrame,
    Input,
    Physics,
    AI,
    Culling,
    Upload,
    Draw,
    Present,
    EndFrame,
}
