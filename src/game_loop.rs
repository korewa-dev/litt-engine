//! Game loop — fixed timestep with sub-stepping, input processing, and frame timing.
//! Integrates ECS, physics, rendering, audio, and profiler into a single update loop.

use std::time::Instant;

/// Game configuration
#[derive(Debug, Clone)]
pub struct GameConfig {
    pub window_title: String,
    pub window_width: u32,
    pub window_height: u32,
    pub fullscreen: bool,
    pub vsync: bool,
    pub max_fps: u32,
    pub physics_hz: f32,
    pub substeps: u32,
    pub enable_profiler: bool,
    pub enable_debug_overlay: bool,
}

impl Default for GameConfig {
    fn default() -> Self {
        Self {
            window_title: "Litt Engine".to_string(),
            window_width: 1280,
            window_height: 720,
            fullscreen: false,
            vsync: true,
            max_fps: 144,
            physics_hz: 60.0,
            substeps: 2,
            enable_profiler: false,
            enable_debug_overlay: false,
        }
    }
}

/// The main game loop
pub struct GameLoop {
    pub config: GameConfig,
    pub running: bool,
    pub frame_count: u64,
    pub elapsed_frames: u64,
    pub last_frame_time: Instant,
    pub accumulator: f64,
    pub frame_start: Instant,
    pub fps: f32,
    pub frame_time_ms: f32,
}

impl Default for GameLoop {
    fn default() -> Self { Self::new() }
}

impl GameLoop {
    /// Create a new game loop
    pub fn new() -> Self {
        Self {
            config: GameConfig::default(),
            running: false,
            frame_count: 0,
            elapsed_frames: 0,
            last_frame_time: Instant::now(),
            accumulator: 0.0,
            frame_start: Instant::now(),
            fps: 0.0,
            frame_time_ms: 0.0,
        }
    }

    /// Create with custom config
    pub fn with_config(config: GameConfig) -> Self {
        Self { config, ..Self::new() }
    }

    /// Start the game loop
    pub fn start(&mut self) {
        self.running = true;
        self.frame_count = 0;
        self.elapsed_frames = 0;
        self.accumulator = 0.0;
        self.frame_start = Instant::now();
        self.fps = 0.0;
        self.frame_time_ms = 0.0;
    }

    /// Stop the game loop
    pub fn stop(&mut self) {
        self.running = false;
    }

    /// Check if the game is running
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// Get current FPS
    pub fn fps(&self) -> f32 {
        self.fps
    }

    /// Get frame time in milliseconds
    pub fn frame_time_ms(&self) -> f32 {
        self.frame_time_ms
    }

    /// Get frame count
    pub fn frame_count(&self) -> u64 {
        self.frame_count
    }

    /// Calculate fixed timestep
    pub fn fixed_timestep(&self) -> f64 {
        1.0 / self.config.physics_hz as f64
    }

    /// Check if should cap FPS
    pub fn should_capped_fps(&self) -> bool {
        self.config.max_fps < u32::MAX
    }

    /// Calculate target frame time
    pub fn target_frame_time_ns(&self) -> u128 {
        if self.should_capped_fps() {
            (1_000_000_000u128 / self.config.max_fps as u128).max(1_000_000u128)
        } else {
            0
        }
    }

    /// Poll input (stub — implemented by platform)
    pub fn poll_input(&mut self) -> bool {
        // Platform-specific input polling
        // Returns true if should quit
        false
    }

    /// Update logic (stub)
    pub fn update(&mut self, dt: f32) {
        self.elapsed_frames += 1;
        self.accumulator += dt as f64;
    }

    /// Render frame (stub)
    pub fn render(&mut self) {
        self.frame_count += 1;
    }

    /// Run the game loop until stop() is called
    pub fn run(&mut self) -> i32 {
        self.start();

        let fixed_dt = self.fixed_timestep();

        while self.running {
            let now = Instant::now();
            let frame_time = now.duration_since(self.last_frame_time).as_nanos() as f64 / 1_000_000_000.0;
            self.last_frame_time = now;

            // Cap accumulator to prevent spiral of death
            if self.accumulator > 0.25 {
                self.accumulator = 0.25;
            }

            // Fixed timestep update
            while self.accumulator >= fixed_dt {
                self.update(fixed_dt as f32);
                self.accumulator -= fixed_dt;
            }

            // Render at variable rate
            self.render();

            // Calculate FPS
            let elapsed = now.duration_since(self.frame_start).as_secs_f32();
            if elapsed >= 0.5 {
                self.fps = (self.frame_count as f32) / elapsed;
                self.frame_time_ms = if self.frame_count > 0 {
                    elapsed * 1000.0 / self.frame_count as f32
                } else {
                    0.0
                };
                self.frame_count = 0;
                self.frame_start = now;
            }

            // Frame rate limiting
            if self.should_capped_fps() {
                let target_ns = self.target_frame_time_ns();
                let elapsed = now.elapsed().as_nanos();
                if elapsed < target_ns {
                    std::thread::sleep(std::time::Duration::from_nanos((target_ns - elapsed) as u64));
                }
            }
        }

        0
    }
}
