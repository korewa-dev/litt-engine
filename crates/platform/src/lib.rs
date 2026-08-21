//! Platform abstraction layer for Windows, Linux, and Android.
//! Minimal window creation and Vulkan surface setup.
//! Also includes AI acceleration backends: MUSA (Moore Threads) and NNAPI (Android).

#![allow(clippy::missing_safety_intrinsic)]

use ash::{extensions::khr, vk::Handle};

#[cfg(target_os = "windows")]
mod win32;
#[cfg(target_os = "windows")]
pub use win32::*;

#[cfg(target_os = "linux")]
mod linux;
#[cfg(target_os = "linux")]
pub use linux::*;

#[cfg(target_os = "android")]
mod android;
#[cfg(target_os = "android")]
pub use android::*;

// AI acceleration backends (platform-independent)
pub mod musa;
pub mod nnapi;

use std::ffi::CString;
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

// =============================================================================
// Platform-specific window creation
// =============================================================================

#[cfg(target_os = "windows")]
pub use win32::Win32Window as PlatformWindow;

#[cfg(target_os = "linux")]
pub use linux::X11Window as PlatformWindow;

#[cfg(target_os = "android")]
pub use android::AndroidWindow as PlatformWindow;

// =============================================================================
// GLFW-like minimal window (optional, kept tiny)
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
        #[cfg(target_os = "windows")] { self.inner.should_close() }
        #[cfg(target_os = "linux")] { self.inner.should_close() }
        #[cfg(target_os = "android")] { false }
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

pub unsafe fn create_surface<'a>(
    instance: &ash::Instance,
    allocator: &ash::extensions::khr::Surface,
    window: &Window,
) -> Result<vk::SurfaceKHR, String> {
    #[cfg(target_os = "windows")]
    {
        let info = vk::Win32SurfaceCreateInfoKHR::builder()
            .hinstance(std::ptr::null())
            .hwnd(window.inner.hwnd() as *mut _)
            .build();
        let surface = allocator.create_win32_surface_khr(&info, None)
            .map_err(|e| format!("Failed to create Win32 surface: {}", e))?;
        Ok(surface)
    }

    #[cfg(target_os = "linux")]
    {
        use ash::vk::XlibSurfaceCreateInfoKHR;
        let info = vk::XlibSurfaceCreateInfoKHR::builder()
            .dpy(window.inner.display())
            .wnd(window.inner.window())
            .build();
        let surface = allocator.create_xlib_surface_khr(&info, None)
            .map_err(|e| format!("Failed to create X11 surface: {}", e))?;
        Ok(surface)
    }

    #[cfg(target_os = "android")]
    {
        use ash::vk::AndroidSurfaceCreateInfoKHR;
        let info = AndroidSurfaceCreateInfoKHR::builder()
            .window(window.inner.a_native_window())
            .build();
        let surface = allocator.create_android_surface_khr(&info, None)
            .map_err(|e| format!("Failed to create Android surface: {}", e))?;
        Ok(surface)
    }
}

// =============================================================================
// Platform-specific implementations
// =============================================================================

#[cfg(target_os = "windows")]
mod win32 {
    use super::*;
    use windows_sys::Win32::Foundation::{HWND, WC, WM_CLOSE, WM_QUIT};
    use windows_sys::Win32::Graphics::Gdi::*;
    use windows_sys::Win32::UI::WindowsAndMessaging::*;
    use std::ptr;

    #[repr(C)]
    pub struct Win32Window {
        instance: HINSTANCE,
        hwnd: HWND,
        closed: bool,
    }

    type HINSTANCE = *mut std::ffi::c_void;

    unsafe extern "system" fn window_proc(
        hwnd: HWND, msg: u32, wparam: usize, lparam: usize
    ) -> isize {
        match msg {
            WM_CLOSE | WM_QUIT => {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                DestroyWindow(hwnd);
                0
            }
            _ => DefWindowProcW(hwnd, msg, wparam, lparam as isize),
        }
    }

    impl Win32Window {
        pub fn new(title: &str, size: WindowSize) -> Option<Self> {
            use windows_sys::Win32::Graphics::Gdi::CreateWindowExW;
            use windows_sys::Win32::UI::WindowsAndMessaging::{
                WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, SW_SHOW,
                CW_USEDEFAULT, IDC_ARROW, DK, MSG, GetMessageW, TranslateMessage, DispatchMessageW,
            };
            use std::ffi::WideCString;

            let hinst = unsafe { windows_sys::Win32::Foundation::GetModuleHandleW(ptr::null()) };
            if hinst.is_null() { return None; }

            let class_name = WideCString::from_str("LittWindow").unwrap();
            let title_wc = WideCString::from_str(title).unwrap();

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(window_proc),
                cbClsExtra: 0,
                cbWndExtra: 0,
                hInstance: hinst,
                hIcon: ptr::null(),
                hCursor: unsafe { LoadCursorW(ptr::null_mut(), IDC_ARROW) },
                hbrBackground: ptr::null_mut() as HBRUSH,
                lpszMenuName: ptr::null(),
                lpszClassName: class_name.as_ptr(),
                hIconSm: ptr::null(),
            };

            if unsafe { RegisterClassExW(&wc) } == 0 {
                return None;
            }

            let hwnd = unsafe {
                CreateWindowExW(
                    0,
                    class_name.as_ptr(),
                    title_wc.as_ptr(),
                    WS_OVERLAPPEDWINDOW & !WS_THICKFRAME & !WS_CAPTION,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    size.width as i32, size.height as i32,
                    ptr::null(), ptr::null(), hinst, ptr::null(),
                )
            };

            if hwnd.is_null() { return None; }

            unsafe { ShowWindow(hwnd, SW_SHOW); }

            Some(Self { instance: hinst, hwnd, closed: false })
        }

        pub fn should_close(&self) -> bool { self.closed }

        pub fn hwnd(&self) -> *mut std::ffi::c_void { self.hwnd as *mut _ }

        pub fn pump_messages(&mut self) {
            let mut msg: MSG = unsafe { std::mem::zeroed() };
            while unsafe { GetMessageW(&mut msg, ptr::null_mut(), 0, 0) } > 0 {
                unsafe { TranslateMessage(&msg); }
                unsafe { DispatchMessageW(&msg); }
            }
            if msg.message == WM_QUIT { self.closed = true; }
        }
    }
}

#[cfg(target_os = "linux")]
mod linux {
    use super::*;
    use std::ffi::CString;
    use std::ptr;

    #[repr(C)]
    pub struct X11Window {
        display: *mut std::ffi::c_void,
        window: u32,
        screen: i32,
        closed: bool,
    }

    impl X11Window {
        pub fn new(title: &str, size: WindowSize) -> Option<Self> {
            // Minimal X11 setup using libc calls
            let display_name = CString::new(":0").ok()?;
            let display = unsafe {
                libc_x11::XOpenDisplay(display_name.as_ptr())
            };
            if display.is_null() { return None; }

            let screen = unsafe { libc_x11::XDefaultScreen(display) };
            let root = unsafe { libc_x11::XRootWindow(display, screen) };

            let class_hint = CString::new("litt").ok()?;
            let title_wc = CString::new(title).ok()?;

            let window = unsafe {
                libc_x11::XCreateSimpleWindow(
                    display, root,
                    100, 100,
                    size.width as i32, size.height as i32,
                    0,
                    0,
                    0,
                )
            };

            if window == 0 { return None; }

            unsafe {
                libc_x11::XStoreName(display, window, title_wc.as_ptr());
                libc_x11::XSelectInput(display, window, libc_x11::ExposeMask | libc_x11::StructureNotifyMask | libc_x11::KeyReleaseMask);
                libc_x11::XMapWindow(display, window);
                libc_x11::XFlush(display);
            }

            Some(Self { display, window, screen, closed: false })
        }

        pub fn should_close(&self) -> bool { self.closed }

        pub fn display(&self) -> *mut std::ffi::c_void { self.display }

        pub fn window(&self) -> u32 { self.window }

        pub fn pump_events(&mut self) {
            unsafe {
                let mut event: libc_x11::XEvent = std::mem::zeroed();
                while libc_x11::XCheckTypedWindowEvent(self.display, self.window, libc_x11::DestroyNotify, &mut event) != 0 {
                    self.closed = true;
                }
            }
        }
    }

    // Minimal X11 symbols we need
    #[allow(non_snake_case)]
    mod libc_x11 {
        use std::ffi::c_void;
        use std::os::raw::c_uint;

        #[repr(C)]
        pub struct XEvent {
            type_: i32,
            serial: u64,
            send_event: i32,
            display: *mut c_void,
            window: c_uint,
            // ... other fields omitted for minimal build
        }

        #[link(name = "X11")]
        extern "C" {
            pub fn XOpenDisplay(name: *const i8) -> *mut c_void;
            pub fn XDefaultScreen(dpy: *mut c_void) -> i32;
            pub fn XRootWindow(dpy: *mut c_void, screen: i32) -> c_uint;
            pub fn XCreateSimpleWindow(
                dpy: *mut c_void,
                parent: c_uint,
                x: i32, y: i32,
                width: i32, height: i32,
                border_width: i32,
                border: c_uint,
                background: c_uint,
            ) -> c_uint;
            pub fn XStoreName(dpy: *mut c_void, w: c_uint, name: *const i8);
            pub fn XSelectInput(dpy: *mut c_void, w: c_uint, event_mask: i32);
            pub fn XMapWindow(dpy: *mut c_void, w: c_uint);
            pub fn XFlush(dpy: *mut c_void);
            pub fn XCheckTypedWindowEvent(
                dpy: *mut c_void, w: c_uint,
                event_type: i32,
                reparsed_event: *mut XEvent,
            ) -> i32;
            pub fn XCloseDisplay(dpy: *mut c_void);
        }

        pub const ExposeMask: i32 = 1 << 0;
        pub const StructureNotifyMask: i32 = 1 << 10;
        pub const KeyReleaseMask: i32 = 1 << 15;
        pub const DestroyNotify: i32 = 17;
    }

    impl Drop for X11Window {
        fn drop(&mut self) {
            unsafe { libc_x11::XCloseDisplay(self.display); }
        }
    }
}

#[cfg(target_os = "android")]
mod android {
    use super::*;
    use android_activity::AndroidApp;

    #[repr(C)]
    pub struct AndroidWindow {
        app: *mut AndroidApp,
        window: *mut std::ffi::c_void,
    }

    impl AndroidWindow {
        pub fn new() -> Option<Self> {
            // Android app is provided by the runtime
            Some(Self {
                app: std::ptr::null_mut(),
                window: std::ptr::null_mut(),
            })
        }

        pub fn init(&mut self, app: *mut AndroidApp) {
            unsafe {
                self.app = app;
                self.window = (*app).window;
            }
        }

        pub fn a_native_window(&self) -> *mut std::ffi::c_void {
            self.window
        }

        pub fn should_close(&self) -> bool {
            unsafe {
                if self.app.is_null() { return false; }
                (*self.app).destroyRequested != 0
            }
        }
    }
}




