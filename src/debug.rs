//! Debug logging module.
//! Windows uses OutputDebugString; other platforms use stderr.

#[cfg(target_os = "windows")]
pub mod debug {
    use windows_sys::Win32::Foundation::HWND;
    use windows_sys::Win32::UI::WindowsAndMessaging::OutputDebugStringW;

    pub fn log(msg: &str) {
        unsafe {
            let s: Vec<u16> = msg.encode_utf16().collect();
            OutputDebugStringW(s.as_ptr() as *const _);
        }
    }
}

#[cfg(not(target_os = "windows"))]
pub mod debug {
    pub fn log(msg: &str) {
        eprintln!("{}", msg);
    }
}

pub use debug::log;
