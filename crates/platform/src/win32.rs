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

unsafe extern "system" fn window_proc(
    hwnd: *mut std::ffi::c_void,
    msg: u32,
    wparam: usize,
    lparam: isize,
) -> isize {
    use windows_sys::Win32::UI::WindowsAndMessaging::{DefWindowProcW, DestroyWindow, WM_CLOSE, WM_QUIT};
    match msg {
        WM_CLOSE | WM_QUIT => {
            DestroyWindow(hwnd);
            0
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}

impl Win32Window {
    pub fn new(title: &str, size: WindowSize) -> Option<Self> {
        use windows_sys::Win32::Graphics::Gdi::{CreateSolidBrush, HBRUSH};
        use windows_sys::Win32::System::LibraryLoader::GetModuleHandleW;
        use windows_sys::Win32::UI::WindowsAndMessaging::{
            CreateWindowExW, DispatchMessageW, GetMessageW, LoadCursorW, RegisterClassExW,
            ShowWindow, TranslateMessage, CS_HREDRAW, CS_VREDRAW, CW_USEDEFAULT, IDC_ARROW,
            MSG, SW_SHOW, WNDCLASSEXW, WS_CAPTION, WS_OVERLAPPEDWINDOW, WS_THICKFRAME,
        };

        let hinst = unsafe { GetModuleHandleW(ptr::null()) };
        if hinst.is_null() {
            return None;
        }

        let class_name = to_wide("LittWindow");
        let title_w = to_wide(title);

        let wc = WNDCLASSEXW {
            cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
            style: (CS_HREDRAW | CS_VREDRAW) as u32,
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
    }

    pub fn hwnd(&self) -> *mut std::ffi::c_void {
        self.hwnd
    }

    /// Pump the Win32 message queue. Sets `closed` on WM_QUIT.
    pub fn pump_messages(&mut self) {
        use windows_sys::Win32::UI::WindowsAndMessaging::{
            DispatchMessageW, GetMessageW, TranslateMessage, MSG, WM_QUIT,
        };
        let mut msg: MSG = unsafe { std::mem::zeroed() };
        while unsafe { GetMessageW(&mut msg, ptr::null_mut(), 0, 0) } > 0 {
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
