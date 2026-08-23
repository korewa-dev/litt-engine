//! Minimal X11 window for Linux.

use super::WindowSize;
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
        let display_name = CString::new(":0").ok()?;
        let display = unsafe { x11::XOpenDisplay(display_name.as_ptr()) };
        if display.is_null() {
            return None;
        }

        let screen = unsafe { x11::XDefaultScreen(display) };
        let root = unsafe { x11::XRootWindow(display, screen) };
        let title_c = CString::new(title).ok()?;

        let window = unsafe {
            x11::XCreateSimpleWindow(
                display,
                root,
                100, 100,
                size.width as i32, size.height as i32,
                0, 0, 0,
            )
        };

        if window == 0 {
            return None;
        }

        unsafe {
            x11::XStoreName(display, window, title_c.as_ptr());
            x11::XSelectInput(display, window, x11::ExposeMask | x11::StructureNotifyMask | x11::KeyReleaseMask);
            x11::XMapWindow(display, window);
            x11::XFlush(display);
        }

        Some(Self { display, window, screen, closed: false })
    }

    pub fn should_close(&self) -> bool { self.closed }

    pub fn display(&self) -> *mut std::ffi::c_void { self.display }

    pub fn window(&self) -> u32 { self.window }

    /// Poll for DestroyNotify events; sets `closed`.
    pub fn pump_events(&mut self) {
        unsafe {
            let mut event: x11::XEvent = std::mem::zeroed();
            while x11::XCheckTypedWindowEvent(self.display, self.window, x11::DestroyNotify, &mut event) != 0 {
                self.closed = true;
            }
        }
    }
}

impl Drop for X11Window {
    fn drop(&mut self) {
        unsafe { x11::XCloseDisplay(self.display) };
    }
}

/// Minimal hand-rolled X11 bindings (avoids the heavy x11 crates).
#[allow(non_snake_case)]
mod x11 {
    use std::ffi::c_void;
    use std::os::raw::c_uint;

    #[repr(C)]
    pub struct XEvent {
        pub type_: i32,
        pub serial: u64,
        pub send_event: i32,
        pub display: *mut c_void,
        pub window: c_uint,
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
            event_return: *mut XEvent,
        ) -> i32;
        pub fn XCloseDisplay(dpy: *mut c_void);
    }

    pub const ExposeMask: i32 = 1 << 15;
    pub const StructureNotifyMask: i32 = 1 << 17;
    pub const KeyReleaseMask: i32 = 1 << 1;
    pub const DestroyNotify: i32 = 17;
}
