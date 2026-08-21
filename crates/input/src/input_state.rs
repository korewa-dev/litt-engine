//! Unified input state — combines keyboard, mouse, and gamepad.

use super::keyboard::{Key, KeyboardState};
use super::mouse::{MouseState, MouseButton, CursorMode};
use super::gamepad::{GamepadState, GamepadButton};

/// Unified input state for the current frame
#[derive(Clone, Debug)]
pub struct InputState {
    pub keyboard: KeyboardState,
    pub mouse: MouseState,
    pub gamepad: GamepadState,
    pub cursor_mode: CursorMode,
    pub frame: u64,
}

impl Default for InputState {
    fn default() -> Self {
        Self {
            keyboard: KeyboardState::new(),
            mouse: MouseState::new(),
            gamepad: GamepadState::new(),
            cursor_mode: CursorMode::Normal,
            frame: 0,
        }
    }
}

impl InputState {
    pub fn new() -> Self { Self::default() }

    /// Check if a key is down
    pub fn key_down(&self, key: Key) -> bool {
        self.keyboard.is_down(key)
    }

    /// Check if a key was just pressed this frame
    pub fn key_pressed(&self, key: Key) -> bool {
        self.keyboard.is_pressed(key)
    }

    /// Check if a key was just released this frame
    pub fn key_released(&self, key: Key) -> bool {
        self.keyboard.is_released(key)
    }

    /// Check if a mouse button is down
    pub fn mouse_down(&self, button: MouseButton) -> bool {
        self.mouse.is_down(button)
    }

    /// Check if a mouse button was just pressed
    pub fn mouse_pressed(&self, button: MouseButton) -> bool {
        self.mouse.is_pressed(button)
    }

    /// Check if a mouse button was just released
    pub fn mouse_released(&self, button: MouseButton) -> bool {
        self.mouse.is_released(button)
    }

    /// Get mouse position
    pub fn mouse_pos(&self) -> (f32, f32) {
        (self.mouse.position.0, self.mouse.position.1)
    }

    /// Get mouse delta
    pub fn mouse_delta(&self) -> (f32, f32) {
        (self.mouse.delta.0, self.mouse.delta.1)
    }

    /// Check if a gamepad button is down
    pub fn gamepad_button_down(&self, button: GamepadButton) -> bool {
        self.gamepad.is_down(button)
    }

    /// Get a gamepad axis value
    pub fn gamepad_axis(&self, axis: super::gamepad::GamepadAxis) -> f32 {
        self.gamepad.axis(axis)
    }

    /// Clear frame-specific state
    pub fn clear_frame(&mut self) {
        self.keyboard.clear_frame();
        self.mouse.clear_frame();
        self.gamepad.clear_frame();
        self.frame += 1;
    }

    /// Check if any input occurred this frame
    pub fn has_input(&self) -> bool {
        !self.keyboard.keys_pressed.is_empty()
            || self.mouse.buttons_pressed.iter().any(|&b| b)
            || !self.gamepad.buttons_pressed.is_empty()
    }
}
