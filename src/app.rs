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
    /// Live GPU backend (Vulkan swapchain frames when available)
    pub backend: Option<Box<dyn crate::graphics::GraphicsBackend>>,
    /// One-time notice when no GPU pipeline is available
    pub warned_no_renderer: bool,
    // ---- Studio mode (chat panel + live viewport) ----
    /// Chat panel state when running as Studio
    pub studio_panel: Option<crate::studio::Panel>,
    /// Background build jobs streaming into the chat
    pub studio_bus: crate::studio::JobBus,
    /// Orbit camera around the world bounds (Studio viewport)
    pub orbit: Option<crate::studio::OrbitCam>,
    /// Asset base path ("assets" normally; "<game>/assets" in Studio)
    pub asset_base: String,
    /// Loaded game directory (Studio)
    pub game_dir: Option<String>,
    /// Re-upload world mesh on next frame
    pub dirty_world: bool,
    /// Re-rasterize chat panel on next frame
    pub dirty_panel: bool,
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

        // Create scene graph -- load the AI-generated world when present.
        // Studio mode loads the target game's world instead.
        let mut scene = SceneGraph::new();
        let mut areas = litt_scene::AreaSystem::new();
        let mut asset_base = "assets".to_string();
        let mut game_dir: Option<String> = None;
        let studio_target = crate::STUDIO_TARGET.get().cloned().flatten();
        let scene_path: String = match &studio_target {
            Some(target) => {
                let dir = resolve_game_dir(target);
                match &dir {
                    Some(d) => {
                        asset_base = format!("{}/assets", d);
                        println!("[studio] target game: {}", d);
                        game_dir = Some(d.clone());
                        format!("{}/assets/scenes/world.lscn.json", d)
                    }
                    None => {
                        eprintln!(
                            "[studio] no game '{}' under Project/ - falling back to engine assets",
                            target
                        );
                        "assets/scenes/world.lscn.json".to_string()
                    }
                }
            }
            None => "assets/scenes/world.lscn.json".to_string(),
        };
        if std::path::Path::new(&scene_path).exists() {
            match litt_scene::load_graph_and_areas_file(&scene_path) {
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

        // Studio chat comes alive only in studio mode
        let studio_panel = studio_target.is_some().then(crate::studio::Panel::default);

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
            backend: None,
            warned_no_renderer: false,
            studio_panel,
            studio_bus: crate::studio::JobBus::new(),
            orbit: None,
            asset_base,
            game_dir,
            dirty_world: true,
            dirty_panel: true,
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

            // Studio chat owns the keyboard when the panel is up
            self.studio_input();

            // Calculate delta time
            let now = std::time::Instant::now();
            let dt = now.duration_since(last_time).as_secs_f32().min(0.05);
            last_time = now;

            // Poll gameplay input (camera) unless a menu owns the keyboard
            if !self.is_studio() {
                self.poll_input();
            }

            // Update systems
            self.update(dt);

            // Studio background jobs + orbit
            self.studio_tick(dt);

            // Render
            self.render();

            // End frame
            self.input.end_frame();
            self.game_loop.frame_count += 1;

            // Frame limiter: honor settings.max_fps so idle Studio doesn't
            // burn the GPU/CPU (vsync also gates, but not all present modes).
            let max = self.config.settings.max_fps.max(15) as f32;
            let budget = std::time::Duration::from_secs_f32(1.0 / max);
            let spent = last_time.elapsed();
            if spent < budget {
                std::thread::sleep(budget - spent);
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

        // Bring up the GPU backend against our window (Vulkan first)
        let (w, h) = self.window.size();
        match crate::graphics::select_backend() {
            Ok(mut backend) => {
                #[cfg(target_os = "windows")]
                backend.set_window(self.window.hwnd() as isize);
                match backend.initialize(w.max(1), h.max(1)) {
                    Ok(()) => {
                        println!(
                            "Renderer: {} on {}",
                            backend.name(),
                            backend.adapter_info()
                        );
                        self.backend = Some(backend);
                    }
                    Err(e) => eprintln!("GPU backend init failed: {e} -- software-only path"),
                }
            }
            Err(e) => eprintln!("No graphics backend: {e}"),
        }

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
        // Trait-level camera (legacy pipelines); the Studio viewport drives
        // its own orbit MVP through set_world_mvp below.
        let camera = self.camera_controls.to_camera(90.0, aspect);

        // Deploy the loaded world natively: real OBJ meshes -> tracer scene.
        // Rebuilt per frame for now; cached once the scene stops changing.
        let base = self.asset_base.clone();
        let (world_scene, stats) = crate::world_bridge::build_render_scene(&self.scene, &base);
        self.bridge_stats = stats;

        // Studio: keep the GPU-side world mesh + chat panel in sync
        if self.is_studio() {
            if let Some(ref mut backend) = self.backend {
                if backend.studio_ready() {
                    if self.dirty_world {
                        let (verts, cam) = crate::studio::scene_to_verts(&world_scene);
                        if let Some(c) = cam {
                            self.orbit = Some(c);
                        }
                        backend.upload_world_mesh(&verts);
                        eprintln!(
                            "[studio] world mesh: {} tris",
                            verts.len() / 18
                        );
                        self.dirty_world = false;
                    }
                    if let Some(orbit) = &self.orbit {
                        // aspect of the viewport slice only
                        let vw = w.saturating_sub(crate::graphics::STUDIO_PANEL_W).max(1);
                        backend.set_world_mvp(orbit.mvp(vw as f32 / h.max(1) as f32));
                    }
                    if self.dirty_panel {
                        if let Some(panel) = &mut self.studio_panel {
                            let scale = 2.0f32.max((h as f32 / 720.0) * 2.0);
                            let verts = panel.raster(
                                crate::graphics::STUDIO_PANEL_W, h, scale);
                            backend.upload_panel_mesh(&verts);
                            self.dirty_panel = false;
                        }
                    }
                }
            }
        }

        // Render through the live GPU swapchain when available
        if self.backend.is_some() {
            if let Err(e) = self.backend.as_mut().unwrap().begin_frame() {
                eprintln!("[gpu] begin_frame: {e}");
            }
            if let Err(e) = self.backend.as_mut().unwrap().render(&world_scene, &camera) {
                eprintln!("[gpu] render: {e}");
            }
            if let Err(e) = self.backend.as_mut().unwrap().present() {
                eprintln!("[gpu] present: {e}");
            }
            let _ = self.backend.as_mut().unwrap().end_frame();
        } else if let Some(ref mut pipeline) = self.path_pipeline {
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
        // GPU first (device waits idle before window teardown)
        if let Some(ref mut backend) = self.backend {
            let _ = backend.shutdown();
        }
        // Save config
        self.config.save().ok();
        // Audio cleanup would go here
    }
}

// ===========================================================================
// Studio mode implementation
// ===========================================================================
impl App {
    pub fn is_studio(&self) -> bool {
        self.studio_panel.is_some()
    }

    /// Capture typed characters + Enter into the chat input line.
    fn studio_input(&mut self) {
        if !self.is_studio() || self.settings_menu.open {
            return;
        }
        // Phase 1: read key transitions (immutable borrow only).
        let (ch, back, enter) = {
            let st = self.input.state();
            let shift = st.key_down(Key::LShift) || st.key_down(Key::RShift);
            let mut ch: Option<char> = None;
            const L: &[Key] = &[
                Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H,
                Key::I, Key::J, Key::K, Key::L, Key::M, Key::N, Key::O, Key::P,
                Key::Q, Key::R, Key::S, Key::T, Key::U, Key::V, Key::W, Key::X,
                Key::Y, Key::Z,
            ];
            for (i, k) in L.iter().enumerate() {
                if st.key_pressed(*k) {
                    let base = b'a' + i as u8;
                    ch = Some(if shift {
                        base as char
                    } else {
                        base.to_ascii_lowercase() as char
                    });
                    break;
                }
            }
            if ch.is_none() {
                const N: &[Key] = &[
                    Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5,
                    Key::Num6, Key::Num7, Key::Num8, Key::Num9, Key::Num0,
                ];
                const UNSHIFTED: [char; 10] =
                    ['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'];
                const SHIFTED: [char; 10] =
                    ['!', '@', '#', '$', '%', '^', '&', '*', '(', ')'];
                for (i, k) in N.iter().enumerate() {
                    if st.key_pressed(*k) {
                        ch = Some(if shift { SHIFTED[i] } else { UNSHIFTED[i] });
                        break;
                    }
                }
            }
            if ch.is_none() && st.key_pressed(Key::Space) {
                ch = Some(' ');
            }
            if ch.is_none() {
                let extra = [
                    (Key::Minus, '-', '_'),
                    (Key::Period, '.', '>'),
                    (Key::Comma, ',', '<'),
                    (Key::Slash, '/', '?'),
                    (Key::Equals, '=', '+'),
                ];
                for (k, plain, shifted) in extra {
                    if st.key_pressed(k) {
                        ch = Some(if shift { shifted } else { plain });
                        break;
                    }
                }
            }
            (ch, st.key_pressed(Key::Backspace), st.key_pressed(Key::Return))
        };
        // Phase 2: mutate the panel.
        if let Some(panel) = &mut self.studio_panel {
            if let Some(c) = ch {
                if panel.input.chars().count() < 120 {
                    panel.input.push(c);
                    self.dirty_panel = true;
                }
            }
            if back {
                panel.input.pop();
                self.dirty_panel = true;
            }
            if enter {
                let line = std::mem::take(&mut panel.input);
                self.dirty_panel = true;
                if !line.trim().is_empty() {
                    panel.log(&format!("> {}", line), crate::studio::Kind::User);
                    self.dispatch_command(line.trim().to_string());
                    self.dirty_panel = true;
                }
            }
        }
    }

    /// Per-frame studio upkeep: caret blink, job results, orbit motion.
    fn studio_tick(&mut self, dt: f32) {
        if !self.is_studio() {
            return;
        }
        if let Some(orbit) = &mut self.orbit {
            orbit.angle += dt * 0.12 * if orbit.spin { 1.0 } else { 0.0 };
        }
        if let Some(panel) = &mut self.studio_panel {
            panel.caret_timer += dt;
            if panel.caret_timer >= 0.5 {
                panel.caret_timer = 0.0;
                panel.caret_on = !panel.caret_on;
                self.dirty_panel = true;
            }
        }
        for msg in self.studio_bus.poll() {
            match msg {
                crate::studio::StudioMsg::Line(line, kind) => {
                    if kind == crate::studio::Kind::Ai || !line.trim().is_empty() {
                        if let Some(panel) = &mut self.studio_panel {
                            panel.log(&line, kind);
                        }
                        self.dirty_panel = true;
                    }
                }
                crate::studio::StudioMsg::Done { ok, game_dir } => {
                    if let Some(panel) = &mut self.studio_panel {
                        panel.log(
                            if ok { "build finished." } else { "build FAILED - see log above." },
                            if ok { crate::studio::Kind::Sys } else { crate::studio::Kind::Err },
                        );
                    }
                    self.dirty_panel = true;
                    let _ = (game_dir, ok);
                }
            }
        }
    }

    /// Route a chat line to the engine toolchain.
    fn dispatch_command(&mut self, line: String) {
        let lower = line.to_lowercase();
        let mut bus = std::mem::take(&mut self.studio_bus);
        macro_rules! say {
            ($($a:tt)*) => {{
                if let Some(p) = &mut self.studio_panel {
                    p.log(&format!($($a)*), crate::studio::Kind::Sys);
                }
            }};
        }
        if lower == "help" {
            say!("commands:");
            say!("  make random                 build a surprise game");
            say!("  make about <anything>       e.g. 'a haunted mall'");
            say!("  load <name>                 open a built game");
            say!("  regen                       re-run gameplay layer");
            say!("  clear | quit");
        } else if lower == "clear" {
            if let Some(p) = &mut self.studio_panel {
                p.lines.clear();
            }
        } else if lower == "quit" || lower == "exit" {
            self.game_loop.stop();
        } else if lower == "make" || lower == "make random" || lower.starts_with("random") {
            if bus.running {
                say!("busy - wait for the current build.");
            } else {
                say!("building a random complete game...");
                bus.build_random();
            }
        } else if let Some(rest) = lower.strip_prefix("load ") {
            let name = rest.trim();
            match resolve_game_dir(name) {
                Some(dir) => {
                    self.reload_game(dir);
                }
                None => say!("no game '{}' under Project/", name),
            }
        } else if lower == "regen" {
            if let Some(dir) = self.game_dir.clone() {
                if bus.running {
                    say!("busy.");
                } else {
                    say!("regenerating gameplay layer...");
                    bus.spawn_tool(
                        "template/tools/worldgen/enrich_game.py",
                        &[
                            "--game-dir".into(), dir.clone(),
                            "--brief".into(), format!("{}/brief.json", dir),
                            "--seed".into(), format!("{}", rand_seed()),
                        ],
                        None,
                    );
                    self.reload_game(dir);
                }
            } else {
                say!("no game loaded.");
            }
        } else if let Some(about) = lower.strip_prefix("make about ") {
            if bus.running {
                say!("busy - wait for the current build.");
            } else {
                say!("interpreting request...");
                bus.build_about(about.trim());
            }
        } else {
            // Free-form text == describe your dream game
            if bus.running {
                say!("busy - wait for the current build.");
            } else {
                say!("reading that as a game brief...");
                bus.build_about(&line);
            }
        }
        self.studio_bus = bus;
    }

    /// Point the Studio at another game directory and refresh everything.
    fn reload_game(&mut self, dir: String) {
        let scene_path = format!("{}/assets/scenes/world.lscn.json", dir);
        match litt_scene::load_graph_and_areas_file(&scene_path) {
            Ok((graph, area_defs)) => {
                let mut areas = litt_scene::AreaSystem::new();
                for a in area_defs {
                    areas.register(a);
                }
                self.scene = graph;
                self.areas = areas;
                self.asset_base = format!("{}/assets", dir);
                self.game_dir = Some(dir.clone());
                self.orbit = None; // rebuilt from fresh bounds
                self.dirty_world = true;
                if let Some(p) = &mut self.studio_panel {
                    p.log(&format!("loaded {}", dir), crate::studio::Kind::Sys);
                }
                self.dirty_panel = true;
                println!("[studio] loaded {}", dir);
            }
            Err(e) => {
                if let Some(p) = &mut self.studio_panel {
                    p.log(&format!("load failed: {}", e), crate::studio::Kind::Err);
                }
            }
        }
    }
}

/// Resolve a game name/path to a directory containing a world.
fn resolve_game_dir(target: &str) -> Option<String> {
    let candidates = [
        target.to_string(),
        format!("Project/{}", target),
        format!("Project/{}.lscn", target),
    ];
    for c in candidates {
        let p = std::path::Path::new(&c).join("assets/scenes/world.lscn.json");
        if p.exists() {
            return Some(c.replace('\\', "/"));
        }
    }
    None
}

fn rand_seed() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.subsec_nanos() as u64 ^ (d.as_secs() << 17))
        .unwrap_or(7)
}


