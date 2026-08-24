//! Application module -- full pipeline integration with all engine systems.

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
use litt_pathtracer::{CameraControls, default_scene, default_camera};
use litt_platform::Window;
use litt_math::*;

/// The main application -- integrates all engine systems
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
    /// FPS-style camera controls (WASD + mouse look)
    pub camera_controls: CameraControls,
    /// Optional renderer (present when GPU is available)
    pub path_pipeline: Option<litt_renderer::RenderPipeline>,
    /// In-game settings menu (Esc)
    pub settings_menu: litt_ui::Menu,
    /// Named world regions tracked for the player position
    pub areas: litt_scene::AreaSystem,
    /// Stats from the last world->renderer deployment
    pub bridge_stats: crate::world_bridge::BridgeStats,
    /// One-time notice when no GPU pipeline is available
    pub warned_no_renderer: bool,
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
        let window = Window::new(title, litt_platform::WindowSize::default())
            .ok_or("Failed to create window")?;

        // Build ECS world
        let world = build_world();

        // Create physics system at 60 Hz
        let physics = PhysicsSystem::at_hz(60.0);

        // Create input system
        let input = InputSystem::new();

        // Create audio context
        let audio = AudioContext::new();

        // Create debug HUD
        let mut hud = DebugHud::new();
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

        // Create scene graph -- load the AI-generated world when present
        let mut scene = SceneGraph::new();
        let mut areas = litt_scene::AreaSystem::new();
        let scene_path = "assets/scenes/world.lscn.json";
        if std::path::Path::new(scene_path).exists() {
            match litt_scene::load_graph_and_areas_file(scene_path) {
                Ok((loaded, area_defs)) => {
                    println!("Scene: {} ({} nodes)", scene_path, loaded.nodes.len() - 1);
                    for a in area_defs {
                        println!("Area: {} (r={:.0})", a.name, a.radius);
                        areas.register(a);
                    }
                    scene = loaded;
                }
                Err(e) => eprintln!("Scene load failed: {}", e),
            }
        }

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
            camera_controls: CameraControls::new(),
            path_pipeline: None,
            settings_menu: litt_ui::Menu::new("SETTINGS"),
            areas,
            bridge_stats: crate::world_bridge::BridgeStats::default(),
            warned_no_renderer: false,
        })
    }

    /// Build the settings menu from current settings.
    fn build_settings_menu(&mut self) {
        let s = &self.config.settings;
        let mut m = litt_ui::Menu::new("SETTINGS");
        m.add_bool("VSync", s.vsync);
        m.add_bool("Fullscreen", s.fullscreen);
        m.add_enum(
            "Max FPS",
            vec!["30", "60", "90", "120", "144", "240"],
            match s.max_fps {
                30 => 0, 60 => 1, 90 => 2, 120 => 3, 144 => 4, _ => 5,
            },
        );
        m.add_float("Master Volume", s.master_volume, 0.0, 1.0, 2);
        m.add_float("Music Volume", s.music_volume, 0.0, 1.0, 2);
        m.add_float("SFX Volume", s.sfx_volume, 0.0, 1.0, 2);
        m.add_bool("Invert Mouse Y", s.invert_y);
        m.add_bool("Ray Tracing", s.ray_tracing);
        m.add_bool("Debug Overlay", s.enable_debug_overlay);
        m.add_action("Apply && Save");
        m.add_action("Quit Engine");
        m.open();
        self.settings_menu = m;
    }

    /// Apply a settings-menu change back onto live settings.
    fn apply_menu_change(&mut self, idx: usize) {
        let s = &mut self.config.settings;
        match idx {
            0 => s.vsync = self.settings_menu.bool_at(0).unwrap_or(s.vsync),
            1 => s.fullscreen = self.settings_menu.bool_at(1).unwrap_or(s.fullscreen),
            2 => {
                if let Some(i) = self.settings_menu.enum_index_at(2) {
                    s.max_fps = [30u32, 60, 90, 120, 144, 240][i.min(5)];
                }
            }
            3 => s.master_volume = self.settings_menu.float_at(3).unwrap_or(s.master_volume),
            4 => s.music_volume = self.settings_menu.float_at(4).unwrap_or(s.music_volume),
            5 => s.sfx_volume = self.settings_menu.float_at(5).unwrap_or(s.sfx_volume),
            6 => s.invert_y = self.settings_menu.bool_at(6).unwrap_or(s.invert_y),
            7 => s.ray_tracing = self.settings_menu.bool_at(7).unwrap_or(s.ray_tracing),
            8 => s.enable_debug_overlay = self.settings_menu.bool_at(8).unwrap_or(s.enable_debug_overlay),
            _ => {}
        }
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
            // Drain OS events into the input system (keys, close, resize)
            self.window.pump_messages();
            let platform_events = self.window.take_events();
            self.input.ingest_platform(&platform_events);

            // Settings menu captures navigation while open; otherwise Esc opens it
            if self.settings_menu.open {
                self.handle_menu_input();
            } else if self.input.state().key_pressed(Key::Escape) {
                self.build_settings_menu();
            }

            // Calculate delta time
            let now = std::time::Instant::now();
            let dt = now.duration_since(last_time).as_secs_f32().min(0.05);
            last_time = now;

            // Poll gameplay input (camera) unless a menu owns the keyboard
            self.poll_input();

            // Update systems
            self.update(dt);

            // Render
            self.render();

            // End frame
            self.input.end_frame();
            self.game_loop.frame_count += 1;
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
        // WASD movement + mouse look (frozen while the menu is open)
        if self.settings_menu.open {
            return;
        }
        // WASD movement + mouse look
        self.camera_controls.process_keyboard(
            &self.input.state.keyboard,
            1.0 / 60.0,
        );
        // Mouse delta from locked cursor (handled by platform crate)
        let (dx, dy) = self.input.state.mouse.delta.into();
        self.camera_controls.process_mouse(dx, dy);
    }

    /// Translate pressed keys into menu navigation while the menu is open.
    fn handle_menu_input(&mut self) {
        use litt_ui::{MenuEvent, MenuInput};
        let st = self.input.state();
        let nav = if st.key_pressed(Key::ArrowUp) {
            Some(MenuInput::Up)
        } else if st.key_pressed(Key::ArrowDown) {
            Some(MenuInput::Down)
        } else if st.key_pressed(Key::ArrowLeft) {
            Some(MenuInput::Left)
        } else if st.key_pressed(Key::ArrowRight) {
            Some(MenuInput::Right)
        } else if st.key_pressed(Key::Return) || st.key_pressed(Key::Space) {
            Some(MenuInput::Select)
        } else if st.key_pressed(Key::Escape) || st.key_pressed(Key::Backspace) {
            Some(MenuInput::Back)
        } else {
            None
        };

        if let Some(nav) = nav {
            match self.settings_menu.handle(nav) {
                MenuEvent::Changed(idx, _) => self.apply_menu_change(idx),
                MenuEvent::Activated(9) => {
                    // "Apply && Save"
                    self.config.settings.sanitize();
                    match self.config.save() {
                        Ok(()) => println!("Settings saved."),
                        Err(e) => eprintln!("Settings save failed: {}", e),
                    }
                }
                MenuEvent::Activated(10) => self.game_loop.stop(), // "Quit Engine"
                MenuEvent::Closed => {
                    // Persist whatever was changed live during browsing.
                    self.config.save().ok();
                }
                _ => {}
            }
        }
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

        // Track area transitions for the player camera position
        let p = self.camera_controls.position;
        if let Some(t) = self.areas.update([p.0, p.1, p.2]) {
            let name = |id: Option<u32>| {
                id.and_then(|id| self.areas.get(id)).map(|a| a.name.as_str()).unwrap_or("wilderness")
            };
            println!("[area] left {} -> entered {}", name(t.left), name(t.entered));
        }

        // Update HUD
        if self.config.settings.enable_debug_overlay {
            let path_samples = if let Some(ref pipeline) = self.path_pipeline {
                pipeline.frame_count
            } else { 0 };
            self.hud.update_stats(
                self.fps_counter,
                self.frame_time_ms,
                0, // draw calls
                0, // triangles
                0.0, // npu latency
            );
            self.hud.path_trace_samples = path_samples;
            self.hud.path_trace_active = self.path_pipeline.is_some();
        }
    }

    /// Render a frame
    fn render(&mut self) {
        let (w, h) = self.window.size();
        let aspect = w as f32 / h.max(1) as f32;
        let camera = self.camera_controls.to_camera(90.0, aspect);

        // Deploy the loaded world natively: real OBJ meshes -> tracer scene.
        // Rebuilt per frame for now; cached once the scene stops changing.
        let (world_scene, stats) = crate::world_bridge::build_render_scene(&self.scene, "assets");
        self.bridge_stats = stats;

        // Render through the Vulkan pipeline if available
        if let Some(ref mut pipeline) = self.path_pipeline {
            pipeline.update(&camera, &world_scene, w, h);
        } else if !self.warned_no_renderer {
            self.warned_no_renderer = true;
            println!(
                "Renderer: no GPU pipeline yet -- world deployed to memory ({} tris, {} markers). Run on a Vulkan device for visuals.",
                self.bridge_stats.triangles, self.bridge_stats.spheres
            );
        }

        // Settings menu draws on the overlay while open
        if self.settings_menu.open {
            self.overlay.clear();
            self.settings_menu.render(&mut self.overlay, 60.0, 60.0, 26.0);
        }
        // swap handled by present
    }

    /// Shutdown all systems
    fn shutdown(&mut self) {
        // Save config
        self.config.save().ok();
        // Audio cleanup would go here
    }
}


