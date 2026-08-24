//! Platform events -- OS input/window events queued by the platform layer.
//!
//! The Win32 message pump pushes raw virtual-key and window events into a
//! process-global queue; the engine main loop drains it each frame with
//! [`take_events`] and feeds it to the input system. A global queue keeps
//! `window_proc` free of user-data plumbing (single primary window is a
//! stated engine constraint) and stays safe across re-entrant message
//! dispatch.

use std::sync::Mutex;

/// Raw platform event. Keyboard events carry native virtual-key codes so
/// higher layers own the mapping to their key model.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PlatformEvent {
    /// Key pressed (native virtual-key code)
    KeyDown { vk: u32 },
    /// Key released (native virtual-key code)
    KeyUp { vk: u32 },
    /// Typed character (from WM_CHAR, layout-aware)
    Char(char),
    /// User asked to close the window (X button / Alt+F4)
    CloseRequested,
    /// Client area resized
    Resize { width: u32, height: u32 },
}

static QUEUE: Mutex<Vec<PlatformEvent>> = Mutex::new(Vec::new());

/// Called by the platform window procedure. Never blocks; grows only between
/// drains, which happen every frame.
pub fn push_event(event: PlatformEvent) {
    if let Ok(mut q) = QUEUE.lock() {
        q.push(event);
    }
}

/// Drain all pending events (oldest first).
pub fn take_events() -> Vec<PlatformEvent> {
    match QUEUE.lock() {
        Ok(mut q) => std::mem::take(&mut *q),
        Err(_) => Vec::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_take_roundtrip_is_fifo() {
        push_event(PlatformEvent::KeyDown { vk: 0x1B });
        push_event(PlatformEvent::Resize { width: 800, height: 600 });
        let drained = take_events();
        assert_eq!(drained[0], PlatformEvent::KeyDown { vk: 0x1B });
        assert_eq!(drained[1], PlatformEvent::Resize { width: 800, height: 600 });
        // Queue is empty after drain.
        assert!(take_events().is_empty());
        take_events(); // also clear anything from other tests for isolation
    }
}
