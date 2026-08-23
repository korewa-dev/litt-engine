//! FPS history -- tracks and visualizes FPS over time.
//! Used for generating FPS graphs and detecting stuttering.

use std::collections::VecDeque;

/// FPS history buffer
#[derive(Debug)]
pub struct FpsHistory {
    pub samples: VecDeque<(u64, f32)>, // (timestamp_ms, fps)
    pub max_samples: usize,
}

impl Default for FpsHistory {
    fn default() -> Self { Self::new() }
}

impl FpsHistory {
    pub fn new() -> Self {
        Self {
            samples: VecDeque::new(),
            max_samples: 300, // 5 minutes at 1fps sample rate
        }
    }

    pub fn with_max_samples(max: usize) -> Self {
        Self {
            samples: VecDeque::new(),
            max_samples: max,
        }
    }

    /// Record an FPS sample
    pub fn record(&mut self, fps: f32, timestamp_ms: u64) {
        self.samples.push_back((timestamp_ms, fps));
        while self.samples.len() > self.max_samples {
            self.samples.pop_front();
        }
    }

    /// Get the FPS curve as a string (ASCII art)
    pub fn to_ascii_graph(&self, width: usize, height: usize) -> String {
        if self.samples.is_empty() { return "No data".to_string(); }

        let min_fps = 0.0;
        let max_fps = self.samples.iter().map(|(_, fps)| fps).cloned().fold(0.0, f32::max);
        let max_fps = max_fps.max(1.0);

        let mut graph = String::new();

        // Sample the data to fit the width
        let step = self.samples.len().max(1) / width;
        for row in (0..height).rev() {
            let threshold = min_fps + (max_fps - min_fps) * row as f32 / height as f32;
            let mut line = String::new();
            let mut col = 0;
            while col < width {
                let start = col * step;
                let end = (col + 1) * step;
                let lo = start.min(self.samples.len());
                let hi = end.min(self.samples.len());
                let n = hi.saturating_sub(lo).max(1);
                let avg = self
                    .samples
                    .iter()
                    .skip(lo)
                    .take(n)
                    .map(|(_, fps)| fps)
                    .sum::<f32>()
                    / n as f32;
                line.push(if avg >= threshold { '#' } else { ' ' });
                col += 1;
            }
            graph.push_str(&line);
            graph.push('\n');
        }

        graph.push_str(&format!("{:.0} fps", max_fps));
        graph
    }

    /// Get statistics
    pub fn stats(&self) -> FpsStats {
        if self.samples.is_empty() {
            return FpsStats::default();
        }

        let fps_values: Vec<f32> = self.samples.iter().map(|(_, fps)| *fps).collect();
        let avg = fps_values.iter().sum::<f32>() / fps_values.len() as f32;
        let min = fps_values.iter().cloned().fold(f32::MAX, f32::min);
        let max = fps_values.iter().cloned().fold(0.0, f32::max);

        // Calculate 1% low (worst 1% of frames)
        let mut sorted = fps_values.clone();
        sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());
        let one_percent_low = sorted.get(sorted.len() / 100).copied().unwrap_or(min);

        // Detect stuttering (fps variance)
        let variance = fps_values.iter().map(|f| (f - avg) * (f - avg)).sum::<f32>() / fps_values.len() as f32;
        let stutter = variance.sqrt();

        FpsStats {
            avg,
            min,
            max,
            one_percent_low,
            stutter,
            sample_count: self.samples.len(),
        }
    }

    /// Get recent FPS values
    pub fn recent(&self, count: usize) -> Vec<f32> {
        let count = count.min(self.samples.len());
        self.samples.iter().rev().take(count).map(|(_, fps)| *fps).collect()
    }
}

/// FPS statistics
#[derive(Debug, Default)]
pub struct FpsStats {
    pub avg: f32,
    pub min: f32,
    pub max: f32,
    pub one_percent_low: f32,
    pub stutter: f32,
    pub sample_count: usize,
}

impl FpsStats {
    pub fn quality(&self) -> &str {
        if self.avg >= 55.0 && self.one_percent_low >= 30.0 && self.stutter < 5.0 {
            "Excellent"
        } else if self.avg >= 45.0 && self.one_percent_low >= 25.0 && self.stutter < 10.0 {
            "Good"
        } else if self.avg >= 30.0 {
            "Playable"
        } else {
            "Poor"
        }
    }
}
