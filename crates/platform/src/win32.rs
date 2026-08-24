//! Win32 window creation for Windows.
//!
//! Type notes (windows-sys 0.59): `HWND`, `HINSTANCE`, `HMODULE`, `HCURSOR`,
//! `HBRUSH` are all `*mut c_void`; `WPARAM` = `usize`, `LPARAM`/`LRESULT` = `isize`.

use super::{to_wide, WindowSize};
use std::ptr;

/// Get the module handle for the current executable.
pub(crate) fn get_module_handle() -> *mut std::ffi::c_void {
    unsafe { windows_sys::Win32::System::LibraryLoader::GetModuleHandleW(ptr::null()) }
}

#[repr(C)]
pub struct Win32Window {
    #[allow(dead_code)]
    instance: *mut std::ffi::c_void,
    hwnd: *mut std::ffi::c_void,
    closed: bool,
}

/// Set by `window_proc` on WM_CLOSE/WM_QUIT. WM_DESTROY (posted by
/// DestroyWindow) never carries WM_QUIT, so a plain message check would
/// miss the X button.
static CLOSE_REQUESTED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

unsafe extern "system" fn window_proc(
    hwnd: *mut std::ffi::c_void,
    msg: u32,
    wparam: usize,
    lparam: isize,
) -> isize {
    use crate::events::{push_event, PlatformEvent};
    use windows_sys::Win32::UI::WindowsAndMessaging::{
        DefWindowProcW, DestroyWindow, WM_CHAR, WM_CLOSE, WM_KEYDOWN, WM_KEYUP,
        WM_QUIT, WM_SIZE, WM_SYSKEYDOWN, WM_SYSKEYUP,
    };
    match msg {
        WM_CLOSE | WM_QUIT => {
            CLOSE_REQUESTED.store(true, std::sync::atomic::Ordering::SeqCst);
            push_event(PlatformEvent::CloseRequested);
            DestroyWindow(hwnd);
            0
        }
        WM_KEYDOWN | WM_SYSKEYDOWN => {
            let vk = wparam as u32;
            // Bit 30 = previous key state -> set on auto-repeat.
            let repeat = ((lparam as u32) & (1 << 30)) != 0;
            if !repeat {
                push_event(PlatformEvent::KeyDown { vk });
            }
            if msg == WM_SYSKEYDOWN {
                DefWindowProcW(hwnd, msg, wparam, lparam)
            } else {
                0
            }
        }
        WM_KEYUP | WM_SYSKEYUP => {
            push_event(PlatformEvent::KeyUp { vk: wparam as u32 });
            0
        }
        WM_CHAR => {
            if let Some(ch) = char::from_u32(wparam as u32) {
                push_event(PlatformEvent::Char(ch));
            }
            0
        }
        WM_SIZE => {
            let w = (lparam as u32) & 0xFFFF;
            let h = ((lparam as u32) >> 16) & 0xFFFF;
            if w > 0 && h > 0 {
                push_event(PlatformEvent::Resize { width: w, height: h });
            }
            DefWindowProcW(hwnd, msg, wparam, lparam)
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}

impl Win32Window {
    pub fn new(title: &str, size: WindowSize) -> Option<Self> {
        use windows_sys::Win32::Graphics::Gdi::{CreateSolidBrush, HBRUSH};
        use windows_sys::Win32::System::LibraryLoader::GetModuleHandleW;
        use windows_sys::Win32::UI::WindowsAndMessaging::{
            CreateWindowExW, LoadCursorW, RegisterClassExW,
            ShowWindow, CS_HREDRAW, CS_VREDRAW, CW_USEDEFAULT, IDC_ARROW, SW_SHOW, WNDCLASSEXW, WS_CAPTION, WS_OVERLAPPEDWINDOW, WS_THICKFRAME,
        };

        let hinst = unsafe { GetModuleHandleW(ptr::null()) };
        if hinst.is_null() {
            return None;
        }

        let class_name = to_wide("LittWindow");
        let title_w = to_wide(title);

        let wc = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            style: (CS_HREDRAW | CS_VREDRAW),
            lpfnWndProc: Some(window_proc),
            cbClsExtra: 0,
            cbWndExtra: 0,
            hInstance: hinst,
            hIcon: ptr::null_mut(),
            hCursor: unsafe { LoadCursorW(ptr::null_mut(), IDC_ARROW) },
            hbrBackground: unsafe { CreateSolidBrush(0x00202020) } as HBRUSH,
            lpszMenuName: ptr::null(),
            lpszClassName: class_name.as_ptr(),
            hIconSm: ptr::null_mut(),
        };

        if unsafe { RegisterClassExW(&wc) } == 0 {
            return None;
        }

        // Borderless: WS_OVERLAPPEDWINDOW without THICKFRAME/CAPTION
        const STYLE: u32 = WS_OVERLAPPEDWINDOW & !(WS_THICKFRAME | WS_CAPTION);

        let hwnd = unsafe {
            CreateWindowExW(
                0,
                class_name.as_ptr(),
                title_w.as_ptr(),
                STYLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                size.width as i32,
                size.height as i32,
                ptr::null_mut(), // hWndParent
                ptr::null_mut(), // hMenu
                hinst,
                ptr::null(),     // lpParam
            )
        };

        if hwnd.is_null() {
            return None;
        }

        unsafe { ShowWindow(hwnd, SW_SHOW) };

        Some(Self { instance: hinst, hwnd, closed: false })
    }

    pub fn should_close(&self) -> bool {
        self.closed
            || CLOSE_REQUESTED.load(std::sync::atomic::Ordering::SeqCst)
    }

    pub fn hwnd(&self) -> *mut std::ffi::c_void {
        self.hwnd
    }

    /// Pump the Win32 message queue without blocking. Sets `closed` on
    /// WM_QUIT / close request so the frame loop keeps its cadence.
    pub fn pump_messages(&mut self) {
        use windows_sys::Win32::UI::WindowsAndMessaging::{
            DispatchMessageW, PeekMessageW, TranslateMessage, MSG, PM_REMOVE, WM_QUIT,
        };
        let mut msg: MSG = unsafe { std::mem::zeroed() };
        // Drain everything currently queued; never wait for new messages.
        while unsafe { PeekMessageW(&mut msg, ptr::null_mut(), 0, 0, PM_REMOVE) } > 0 {
            unsafe {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if msg.message == WM_QUIT {
            self.closed = true;
        }
    }
}
