//! Application module — full pipeline integration with all engine systems.

use super::game_loop::*;
use crate::version;
use crate::template::components::*;
use crate::ecs::build_world;
use crate::ecs::PhysicsTransformSyncSystem;
use litt_ecs::*;
use litt_physics::*;
use litt_ai::*;
use litt_asset::*;
use litt_input::*;
use litt_audio::*;
use litt_ui::*;
use litt_profiler::*;
use litt_scene::*;
use litt_config::*;
use litt_platform::Window;
use litt_math::*;

/// The main application — integrates all engine systems
pub struct App {
    pub window: Window,
    pub game_loop: GameLoop,
    pub world: World,
    pub physics: PhysicsSystem,
    pub input: InputSystem,
    pub audio: AudioContext,
    pub hud: DebugHud,
    pub overlay: Overlay,
    pub profiler: FrameTimer,
    pub scene: SceneGraph,
    pub config: ConfigManager,
    pub asset_manager: AssetManager,
    pub should_quit: bool,
    pub fps_counter: f32,
    pub frame_time_ms: f32,
}

impl App {
    /// Create a new application
    #[cfg(target_os = "windows")]
    pub fn new(hInstance: *mut std::ffi::c_void, nCmdShow: i32) -> Result<Self, String> {
        Self::create("Litt Engine", hInstance, nCmdShow)
    }

    /// Create a new application (Linux)
    #[cfg(target_os = "linux")]
    pub fn new() -> Result<Self, String> {
        Self::create("Litt Engine", std::ptr::null_mut(), 0)
    }

    /// Create from Android
    #[cfg(target_os = "android")]
    pub fn from_android(_app: *mut android_activity::AndroidApp) -> Result<Self, String> {
        Self::create("Litt Engine", std::ptr::null_mut(), 0)
    }

    fn create(title: &str, _hInstance: *mut std::ffi::c_void, _nCmdShow: i32) -> Result<Self, String> {
        // Create window
        let window = Window::new(title, WindowSize { width: 1280, height: 720 })
            .ok_or("Failed to create window")?;

        // Build ECS world
        let world = build_world();

        // Create physics system at 60 Hz
        let mut physics = PhysicsSystem::at_hz(60.0);

        // Create input system
        let input = InputSystem::new();

        // Create audio context
        let audio = AudioContext::new();

        // Create debug HUD
        let hud = DebugHud::new();
        hud.enabled = true;

        // Create overlay
        let overlay = Overlay::new();

        // Create profiler
        let profiler = FrameTimer::new();

        // Create config manager
        let mut config = ConfigManager::new();
        config.load().ok();

        // Create game loop
        let game_loop = GameLoop::with_config(GameConfig {
            window_title: title.to_string(),
            window_width: config.settings.window_width,
            window_height: config.settings.window_height,
            fullscreen: config.settings.fullscreen,
            vsync: config.settings.vsync,
            max_fps: config.settings.max_fps,
            physics_hz: 60.0,
            substeps: 2,
            enable_profiler: config.settings.enable_profiler,
            enable_debug_overlay: config.settings.enable_debug_overlay,
        });

        // Create scene graph
        let scene = SceneGraph::new();

        // Create asset manager
        let asset_manager = AssetManager::new()
            .with_base_path("assets")
            .with_cache_size(512 * 1024 * 1024);

        Ok(Self {
            window,
            game_loop,
            world,
            physics,
            input,
            audio,
            hud,
            overlay,
            profiler,
            scene,
            config,
            asset_manager,
            should_quit: false,
            fps_counter: 0.0,
            frame_time_ms: 0.0,
        })
    }

    /// Run the application main loop
    pub fn run(mut self) -> i32 {
        println!("{} v{}", version::NAME, version::VERSION);
        println!("Build: {} | Commit: {}", version::BUILD_DATE, version::GIT_COMMIT);
        println!();

        // Initialize systems
        if let Err(e) = self.init() {
            eprintln!("Failed to initialize: {}", e);
            return 1;
        }

        // Main loop
        self.game_loop.start();
        let mut last_time = std::time::Instant::now();

        while self.game_loop.is_running() && !self.window.should_close() {
            // Poll input
            self.poll_input();

            // Calculate delta time
            let now = std::time::Instant::now();
            let dt = now.duration_since(last_time).as_secs_f32().min(0.05);
            last_time = now;

            // Update systems
            self.update(dt);

            // Render
            self.render();

            // End frame
            self.input.end_frame();
            self.game_loop.frame_count += 1;

            // Check for quit
            if self.input.key_pressed(Key::Escape) {
                self.game_loop.stop();
            }
        }

        // Cleanup
        self.shutdown();

        0
    }

    /// Initialize all systems
    fn init(&mut self) -> Result<(), String> {
        // Init audio
        self.audio.init().ok();

        // Print GPU info
        println!("Initializing engine...");
        Ok(())
    }

    /// Poll input events
    fn poll_input(&mut self) {
        // Platform-specific input polling would go here
        // This is a stub — full implementation uses platform crate
    }

    /// Update all systems
    fn update(&mut self, dt: f32) {
        // Update physics
        self.physics.update(&mut self.world, dt);

        // Sync physics transforms back to render transforms
        for system in &mut [PhysicsTransformSyncSystem] {
            system.update(&mut self.world, dt);
        }

        // Update input
        self.input.end_frame();

        // Update audio
        self.audio.update(dt);

        // Update profiler
        self.profiler.record_frame();
        self.fps_counter = self.profiler.fps;
        self.frame_time_ms = self.profiler.last_frame_ms;

        // Update HUD
        if self.config.settings.enable_debug_overlay {
            self.hud.update_stats(
                self.fps_counter,
                self.frame_time_ms,
                0, // draw calls
                0, // triangles
                0.0, // npu latency
            );
        }
    }

    /// Render a frame
    fn render(&mut self) {
        // Render would be implemented here with the Vulkan/DX12 pipeline
        // For now, just present the window
        self.window.swap_buffers();
    }

    /// Shutdown all systems
    fn shutdown(&mut self) {
        // Save config
        self.config.save().ok();
        // Audio cleanup would go here
    }
}
