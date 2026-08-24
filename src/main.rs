//! Litt Engine - Ultra-lightweight Vulkan path tracing engine.
//!
//! Targets: Windows, Linux, Android
//! GPU Focus: AMD (RDNA2/RDNA3/RRNA4), Intel Arc, Samsung Exynos
//! Features: VMA Memory Management, BLAS/TLAS Pipeline, FSR 3.1.5, NPU acceleration
//!
//! Engine Modules: input, audio, ui, profiler, scene, config

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(unused_variables)]

extern crate alloc;

// =============================================================================
// Re-export all crates
// =============================================================================
pub mod version;
pub use litt_math::*;
pub use litt_platform::*;
pub use litt_vulkan::*;
pub use litt_renderer::*;
pub use litt_pathtracer::*;
pub use litt_fidelityfx::*;
pub use litt_ecs::*;
pub use litt_physics::*;
pub use litt_ai::*;
pub use litt_asset::*;
pub use litt_input::*;
pub use litt_audio::*;
pub use litt_ui::*;
pub use litt_profiler::*;
pub use litt_scene::*;
pub use litt_config::*;

// =============================================================================
// Internal modules
// =============================================================================
pub mod ecs;
pub mod graphics;
pub mod template;
mod app;
mod debug;
pub mod editor;
mod logging;
mod game_loop;
pub mod world_bridge;

use app::*;
use logging::*;
use game_loop::*;

/// Main entry point
fn print_version() {
    println!("{} v{} (build {})", version::NAME, version::VERSION, version::GIT_COMMIT);
}

/// CLI dispatch: `litt edit [scene]` opens the editor; other args run the game.
/// Returns Some(exit_code) when a subcommand consumed the invocation.
fn run_cli() -> Option<i32> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.first().map(|s| s.as_str()) {
        Some("edit") => {
            let path = args.get(1).cloned().unwrap_or_else(|| "untitled.lscn.json".to_string());
            print_version();
            match editor::run_interactive(&path) {
                Ok(()) => Some(0),
                Err(e) => {
                    eprintln!("editor failed: {}", e);
                    Some(1)
                }
            }
        }
        Some("--help") | Some("-h") | Some("help") => {
            println!("Usage: litt [edit <scene.lscn.json>]");
            println!("  (no args)          run the game");
            println!("  edit <scene>       open the scene editor (creates file when missing)");
            Some(0)
        }
        _ => None,
    }
}

#[cfg(target_os = "windows")]
#[no_mangle]
pub extern "system" fn wWinMain(
    hInstance: *mut std::ffi::c_void,
    _hPrevInstance: *mut std::ffi::c_void,
    _lpCmdLine: *mut std::ffi::c_void,
    nCmdShow: i32,
) -> i32 {
    #[cfg(feature = "log-std")]
    logging::init();

    let app = match App::new(hInstance, nCmdShow) {
        Ok(a) => a,
        Err(e) => {
            debug::log(&format!("Failed to create app: {}", e));
            return 1;
        }
    };

    app.run()
}

#[cfg(target_os = "linux")]
#[no_mangle]
pub extern "C" fn main() -> i32 {
    #[cfg(feature = "log-std")]
    logging::init();

    let app = match App::new() {
        Ok(a) => a,
        Err(e) => {
            debug::log(&format!("Failed to create app: {}", e));
            return 1;
        }
    };

    app.run()
}

#[cfg(target_os = "android")]
#[no_mangle]
pub extern "C" fn android_main(app: *mut android_activity::AndroidApp) {
    #[cfg(feature = "log-std")]
    logging::init();

    let _app = match App::from_android(app) {
        Ok(a) => a,
        Err(e) => {
            debug::log(&format!("Failed to create app: {}", e));
            return;
        }
    };

    _app.run();
}

/// Standard entry point (console subsystem builds).
#[cfg(target_os = "windows")]
fn main() -> std::process::ExitCode {
    #[cfg(feature = "log-std")]
    logging::init();

    if let Some(code) = run_cli() {
        return std::process::ExitCode::from(code as u8);
    }

    let code = wWinMain(std::ptr::null_mut(), std::ptr::null_mut(), std::ptr::null_mut(), 1);
    std::process::ExitCode::from(code as u8)
}
