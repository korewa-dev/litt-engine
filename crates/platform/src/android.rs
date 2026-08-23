//! Android Native Activity window stub.

/// Android window backed by a raw ANativeWindow pointer.
#[repr(C)]
pub struct AndroidWindow {
    app: *mut std::ffi::c_void,
    window: *mut std::ffi::c_void,
}

impl AndroidWindow {
    pub fn new() -> Option<Self> {
        Some(Self {
            app: std::ptr::null_mut(),
            window: std::ptr::null_mut(),
        })
    }

    /// Attach the native activity and grab its ANativeWindow.
    pub fn init(&mut self, app: *mut std::ffi::c_void, native_window: *mut std::ffi::c_void) {
        self.app = app;
        self.window = native_window;
    }

    pub fn a_native_window(&self) -> *mut std::ffi::c_void {
        self.window
    }

    pub fn should_close(&self) -> bool {
        // Real lifecycle handling arrives with the android-activity glue crate.
        false
    }
}
