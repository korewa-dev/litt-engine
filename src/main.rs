//! Litt Engine - Ultra-lightweight Vulkan path tracing engine.
//!
//! Targets: Windows, Linux, Android
//! GPU Focus: AMD (RDNA2/RDNA3)
//! Max Binary Size: 1 MB
//!
//! Features:
//! - Vulkan 1.3 ray tracing
//! - Path tracing with Russian roulette
//! - AMD FidelityFX (FSR 2, CAS)
//! - Minimal dependencies

#![no_std]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![allow(unused_variables)]

extern crate alloc;

use alloc::vec::Vec;
use alloc::string::String;

// Re-export crates
pub use litt_math::*;
pub use litt_platform::*;
pub use litt_vulkan::*;
pub use litt_renderer::*;
pub use litt_pathtracer::*;
pub use litt_fidelityfx::*;

mod app;
mod debug;
mod logging;

use app::*;
use logging::*;

/// Main entry point
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

/// Minimal logging
mod app {
    use super::*;
    use litt_platform::Window;
    use litt_vulkan::*;

    pub struct App {
        window: Window,
        renderer: Option<Renderer>,
        scene: Scene,
        camera: Camera,
        should_quit: bool,
    }

    impl App {
        #[cfg(target_os = "windows")]
        pub fn new(hInstance: *mut std::ffi::c_void, nCmdShow: i32) -> Result<Self, String> {
            let window = Window::new("Litt Engine", WindowSize { width: 1280, height: 720 })
                .ok_or("Failed to create window")?;

            // Vulkan initialization happens lazily
            Ok(Self {
                window,
                renderer: None,
                scene: Scene::default_test_scene(),
                camera: Camera {
                    position: Vec3::new(0.0, 2.0, 5.0),
                    rotation: Vec2::new(0.0, 0.0),
                    fov: core::f32::consts::PI / 3.0,
                    near_plane: 0.1,
                    far_plane: 100.0,
                    aspect: 16.0 / 9.0,
                    exposure: 1.0,
                    _pad: [0.0; 3],
                },
                should_quit: false,
            })
        }

        #[cfg(target_os = "linux")]
        pub fn new() -> Result<Self, String> {
            let window = Window::new("Litt Engine", WindowSize { width: 1280, height: 720 })
                .ok_or("Failed to create window")?;

            Ok(Self {
                window,
                renderer: None,
                scene: Scene::default_test_scene(),
                camera: Camera {
                    position: Vec3::new(0.0, 2.0, 5.0),
                    rotation: Vec2::new(0.0, 0.0),
                    fov: core::f32::consts::PI / 3.0,
                    near_plane: 0.1,
                    far_plane: 100.0,
                    aspect: 16.0 / 9.0,
                    exposure: 1.0,
                    _pad: [0.0; 3],
                },
                should_quit: false,
            })
        }

        #[cfg(target_os = "android")]
        pub fn from_android(_app: *mut android_activity::AndroidApp) -> Result<Self, String> {
            Ok(Self {
                window: Window::new(WindowSize { width: 1280, height: 720 }).ok_or("Failed")?,
                renderer: None,
                scene: Scene::default_test_scene(),
                camera: Camera {
                    position: Vec3::new(0.0, 2.0, 5.0),
                    rotation: Vec2::new(0.0, 0.0),
                    fov: core::f32::consts::PI / 3.0,
                    near_plane: 0.1,
                    far_plane: 100.0,
                    aspect: 16.0 / 9.0,
                    exposure: 1.0,
                    _pad: [0.0; 3],
                },
                should_quit: false,
            })
        }

        pub fn run(mut self) -> i32 {
            // Initialize Vulkan
            match unsafe { self.initialize_vulkan() } {
                Ok(_) => {},
                Err(e) => {
                    debug::log(&format!("Vulkan init failed: {}", e));
                    return 1;
                }
            }

            // Main loop
            while !self.should_quit && !self.window.should_close() {
                self.window.pump_messages();

                if let Some(ref mut renderer) = self.renderer {
                    match self.render_frame(renderer) {
                        Ok(_) => {},
                        Err(e) => {
                            debug::log(&format!("Render error: {}", e));
                            break;
                        }
                    }
                }
            }

            if let Some(ref mut renderer) = self.renderer {
                unsafe {
                    renderer.device.device.wait_idle().ok();
                }
            }

            0
        }

        unsafe fn initialize_vulkan(&mut self) -> Result<(), String> {
            let instance = create_vulkan_instance()?;
            let surface = create_surface(&instance, &instance.enumerate_physical_devices()?.first().cloned().ok_or("No GPU")?, &self.window)?;

            let queue_families = find_queue_families(&instance, *instance.enumerate_physical_devices()?.first().ok_or("No GPU")?)?;
            let device = VulkanDevice::new(&instance, *instance.enumerate_physical_devices()?.first().ok_or("No GPU")?, surface, &queue_families)?;

            let swapchain = create_swapchain(
                &device.device,
                device.physical_device,
                surface,
                &queue_families,
                &device.surface_loader,
                &device.swapchain_loader,
                self.window.size.width,
                self.window.size.height,
            )?;

            let command_pool = CommandPool::new(&device.device, device.graphics_family)?;
            let render_pass = RenderPass::new(&device.device, swapchain.format)?;
            let descriptor_pool = DescriptorPool::new(&device.device, 256)?;

            self.renderer = Some(Renderer {
                device,
                swapchain,
                command_pool,
                render_pass,
                frame_in_flight: 2,
                fences: Vec::new(),
                semaphores: Vec::new(),
                descriptor_pool,
                current_frame: 0,
            });

            Ok(())
        }

        fn render_frame(&mut self, renderer: &mut Renderer) -> Result<(), String> {
            // TODO: Implement actual render loop
            Ok(())
        }
    }
}

mod debug {
    #[cfg(target_os = "windows")]
    pub fn log(msg: &str) {
        unsafe {
            use windows_sys::Win32::Foundation::HWND;
            use windows_sys::Win32::UI::WindowsAndMessaging::OutputDebugStringW;
            let s: Vec<u16> = msg.encode_utf16().collect();
            OutputDebugStringW(s.as_ptr() as *const _);
        }
    }

    #[cfg(not(target_os = "windows"))]
    pub fn log(msg: &str) {
        eprintln!("{}", msg);
    }
}

mod logging {
    pub fn init() {}
}
