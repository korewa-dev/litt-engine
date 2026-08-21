//! Mouse input handling.
//! Position, delta, buttons, and scroll.

use litt_math::Vec2;

/// Mouse button
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum MouseButton {
    Left,
    Right,
    Middle,
    Button4,
    Button5,
}

impl MouseButton {
    pub fn index(&self) -> u32 {
        match self {
            Self::Left => 0,
            Self::Right => 1,
            Self::Middle => 2,
            Self::Button4 => 3,
            Self::Button5 => 4,
        }
    }
}

/// Mouse state
#[derive(Clone, Debug, Default)]
pub struct MouseState {
    /// Position in pixels
    pub position: Vec2,
    /// Position change this frame
    pub delta: Vec2,
    /// Button down this frame
    pub buttons_down: [bool; 5],
    /// Button pressed this frame
    pub buttons_pressed: [bool; 5],
    /// Button released this frame
    pub buttons_released: [bool; 5],
    /// Scroll offset
    pub scroll: Vec2,
    /// Cursor visible
    pub visible: bool,
    /// Cursor locked to window
    pub locked: bool,
}

impl MouseState {
    pub fn new() -> Self { Self::default() }

    pub fn is_down(&self, button: MouseButton) -> bool {
        self.buttons_down[button.index() as usize]
    }

    pub fn is_pressed(&self, button: MouseButton) -> bool {
        self.buttons_pressed[button.index() as usize]
    }

    pub fn is_released(&self, button: MouseButton) -> bool {
        self.buttons_released[button.index() as usize]
    }

    pub fn clear_frame(&mut self) {
        self.buttons_pressed.fill(false);
        self.buttons_released.fill(false);
    }
}

/// Cursor modes
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CursorMode {
    /// Visible, moves freely
    Normal,
    /// Hidden, moves freely
    Hidden,
    /// Locked to window center
    Locked,
    /// Hidden, can't leave window
    Captured,
}
