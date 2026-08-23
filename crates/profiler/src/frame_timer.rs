//! Frame timing -- measures frame duration and FPS.

use std::time::Instant;

/// Frame timer
#[derive(Debug)]
pub struct FrameTimer {
    pub start: Instant,
    pub last_frame_ms: f32,
    pub avg_frame_ms: f32,
    pub min_frame_ms: f32,
    pub max_frame_ms: f32,
    pub fps: f32,
    pub frame_count: u32,
    pub times: Vec<f32>,
}

impl Default for FrameTimer {
    fn default() -> Self { Self::new() }
}

impl FrameTimer {
    pub fn new() -> Self {
        Self {
            start: Instant::now(),
            last_frame_ms: 0.0,
            avg_frame_ms: 0.0,
            min_frame_ms: f32::MAX,
            max_frame_ms: 0.0,
            fps: 0.0,
            frame_count: 0,
            times: Vec::new(),
        }
    }

    /// Record a frame
    pub fn record_frame(&mut self) {
        let elapsed = self.start.elapsed().as_secs_f32() * 1000.0;
        self.last_frame_ms = elapsed;
        self.frame_count += 1;
        self.times.push(elapsed);

        // Keep last 60 frames for averaging
        if self.times.len() > 60 {
            self.times.remove(0);
        }

        self.avg_frame_ms = self.times.iter().sum::<f32>() / self.times.len() as f32;
        self.min_frame_ms = self.times.iter().cloned().fold(f32::MAX, f32::min);
        self.max_frame_ms = self.times.iter().cloned().fold(0.0, f32::max);
        self.fps = 1000.0 / self.last_frame_ms.max(0.1);
    }

    /// Reset
    pub fn reset(&mut self) {
        self.start = Instant::now();
        self.last_frame_ms = 0.0;
        self.avg_frame_ms = 0.0;
        self.min_frame_ms = f32::MAX;
        self.max_frame_ms = 0.0;
        self.fps = 0.0;
        self.frame_count = 0;
        self.times.clear();
    }
}
