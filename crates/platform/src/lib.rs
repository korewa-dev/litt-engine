//! Platform abstraction layer for Windows, Linux, and Android.
//! Minimal window creation and Vulkan surface setup.
//! Also includes AI acceleration backends: MUSA (Moore Threads) and NNAPI (Android).

#![allow(clippy::missing_safety_intrinsic)]

#[cfg(target_os = "windows")]
pub mod win32;
#[cfg(target_os = "linux")]
pub mod linux;
#[cfg(target_os = "android")]
pub mod android;

// AI acceleration backends (platform-independent)
pub mod musa;
pub mod nnapi;

#[cfg(target_os = "windows")]
pub use win32::Win32Window as PlatformWindow;
#[cfg(target_os = "windows")]
pub use win32::Win32Window;

#[cfg(target_os = "linux")]
pub use linux::X11Window as PlatformWindow;
#[cfg(target_os = "linux")]
pub use linux::X11Window;

#[cfg(target_os = "android")]
pub use android::AndroidWindow as PlatformWindow;
#[cfg(target_os = "android")]
pub use android::AndroidWindow;

use ash::vk;
use bytemuck::{Pod, Zeroable};

// =============================================================================
// Window Handle (platform-specific)
// =============================================================================

#[derive(Clone, Copy, Debug, Pod, Zeroable)]
#[repr(C)]
pub struct WindowSize {
    pub width: u32,
    pub height: u32,
}

impl Default for WindowSize {
    fn default() -> Self { Self { width: 1280, height: 720 } }
}

/// Encode a UTF-8 string as a null-terminated UTF-16 wide string (Win32).
#[cfg(target_os = "windows")]
pub(crate) fn to_wide(s: &str) -> Vec<u16> {
    use std::os::windows::ffi::OsStrExt;
    std::ffi::OsStr::new(s)
        .encode_wide()
        .chain(std::iter::once(0))
        .collect()
}

// =============================================================================
// GLFW-like minimal window
// =============================================================================

/// Opaque window handle - actual type is platform-specific
pub struct Window {
    #[cfg(target_os = "windows")]
    pub inner: Win32Window,
    #[cfg(target_os = "linux")]
    pub inner: X11Window,
    #[cfg(target_os = "android")]
    pub inner: AndroidWindow,
    pub size: WindowSize,
    pub title: String,
}

impl Window {
    #[cfg(target_os = "windows")]
    pub fn new(title: &str, size: WindowSize) -> Option<Self> {
        Win32Window::new(title, size).map(|inner| Self { inner, size, title: title.to_string() })
    }

    #[cfg(target_os = "linux")]
    pub fn new(title: &str, size: WindowSize) -> Option<Self> {
        X11Window::new(title, size).map(|inner| Self { inner, size, title: title.to_string() })
    }

    #[cfg(target_os = "android")]
    pub fn new(_size: WindowSize) -> Option<Self> {
        AndroidWindow::new().map(|inner| Self { inner, size: _size, title: "Litt".to_string() })
    }

    pub fn should_close(&self) -> bool {
        #[cfg(target_os = "windows")]
        { self.inner.should_close() }
        #[cfg(target_os = "linux")]
        { self.inner.should_close() }
        #[cfg(target_os = "android")]
        { false }
    }

    pub fn size(&self) -> (u32, u32) {
        (self.size.width, self.size.height)
    }

    #[cfg(target_os = "windows")]
    pub fn hwnd(&self) -> *mut std::ffi::c_void {
        self.inner.hwnd()
    }
}

// =============================================================================
// Vulkan Surface creation
// =============================================================================

/// Create a Vulkan surface for the given platform window.
///
/// # Safety
/// Caller must guarantee `instance` and `surface_loader` are valid and that
/// the window outlives the returned surface.
pub unsafe fn create_surface(
    entry: &ash::Entry,
    instance: &ash::Instance,
    window: &Window,
) -> Result<vk::SurfaceKHR, String> {
    #[cfg(target_os = "windows")]
    {
        let w32 = ash::khr::win32_surface::Instance::new(entry, instance);
        let info = vk::Win32SurfaceCreateInfoKHR {
            // ash aliases HINSTANCE/HWND as isize; windows-sys uses *mut c_void
            hinstance: win32::get_module_handle() as isize,
            hwnd: window.hwnd() as isize,
            ..Default::default()
        };
        w32.create_win32_surface(&info, None)
            .map_err(|e| format!("Failed to create Win32 surface: {}", e))
    }

    #[cfg(target_os = "linux")]
    {
        let xlib = ash::khr::xlib_surface::Instance::new(entry, instance);
        let info = vk::XlibSurfaceCreateInfoKHR {
            dpy: window.display() as *mut _,
            wnd: window.window(),
            ..Default::default()
        };
        xlib.create_xlib_surface(&info, None)
            .map_err(|e| format!("Failed to create X11 surface: {}", e))
    }

    #[cfg(target_os = "android")]
    {
        let android_loader = ash::khr::android_surface::Instance::new(entry, instance);
        let info = vk::AndroidSurfaceCreateInfoKHR {
            window: window.a_native_window() as *mut _,
            ..Default::default()
        };
        android_loader.create_android_surface(&info, None)
            .map_err(|e| format!("Failed to create Android surface: {}", e))
    }
}

/// Destroy a previously created surface.
///
/// # Safety
/// See `create_surface`.
#[cfg(target_os = "windows")]
pub unsafe fn destroy_surface(
    entry: &ash::Entry,
    instance: &ash::Instance,
    surface: vk::SurfaceKHR,
) {
    let loader = ash::khr::surface::Instance::new(entry, instance);
    loader.destroy_surface(surface, None);
}
