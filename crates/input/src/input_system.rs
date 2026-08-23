//! Input system -- handles platform input events and updates InputState.

use super::input_state::InputState;
use super::keyboard::Key;
use super::mouse::{MouseState, MouseButton};
use super::gamepad::GamepadState;

/// Input system -- bridges platform events to logical input
pub struct InputSystem {
    pub state: InputState,
    pub sensitivity: f32,
    pub scroll_sensitivity: f32,
}

impl Default for InputSystem {
    fn default() -> Self {
        Self {
            state: InputState::new(),
            sensitivity: 0.002,
            scroll_sensitivity: 1.0,
        }
    }
}

impl InputSystem {
    pub fn new() -> Self { Self::default() }

    pub fn with_sensitivity(mut self, sensitivity: f32) -> Self {
        self.sensitivity = sensitivity;
        self
    }

    /// Process a keyboard key down event
    pub fn on_key_down(&mut self, key: Key) {
        if !self.state.keyboard.keys_down.contains(&key) {
            self.state.keyboard.keys_pressed.push(key);
        }
        self.state.keyboard.keys_down.push(key);
    }

    /// Process a keyboard key up event
    pub fn on_key_up(&mut self, key: Key) {
        self.state.keyboard.keys_down.retain(|&k| k != key);
        self.state.keyboard.keys_released.push(key);
    }

    /// Process a mouse button down event
    pub fn on_mouse_down(&mut self, button: MouseButton) {
        let idx = button.index() as usize;
        if idx < self.state.mouse.buttons_down.len() {
            self.state.mouse.buttons_down[idx] = true;
            if !self.state.mouse.buttons_pressed[idx] {
                self.state.mouse.buttons_pressed[idx] = true;
            }
        }
    }

    /// Process a mouse button up event
    pub fn on_mouse_up(&mut self, button: MouseButton) {
        let idx = button.index() as usize;
        if idx < self.state.mouse.buttons_down.len() {
            self.state.mouse.buttons_down[idx] = false;
            self.state.mouse.buttons_released[idx] = true;
        }
    }

    /// Process mouse movement
    pub fn on_mouse_move(&mut self, x: f32, y: f32) {
        self.state.mouse.delta = (x, y).into();
        self.state.mouse.position = (x, y).into();
    }

    /// Process mouse scroll
    pub fn on_mouse_scroll(&mut self, dx: f32, dy: f32) {
        self.state.mouse.scroll.0 += dx * self.scroll_sensitivity;
        self.state.mouse.scroll.1 += dy * self.scroll_sensitivity;
    }

    /// Process gamepad connection
    pub fn on_gamepad_connect(&mut self) {
        self.state.gamepad.connected = true;
    }

    /// Process gamepad disconnect
    pub fn on_gamepad_disconnect(&mut self) {
        self.state.gamepad.connected = false;
        self.state.gamepad = GamepadState::new();
    }

    /// Process a gamepad button
    pub fn on_gamepad_button(&mut self, button_idx: u32, pressed: bool) {
        let idx = button_idx as usize;
        while self.state.gamepad.buttons_down.len() <= idx {
            self.state.gamepad.buttons_down.push(false);
            self.state.gamepad.buttons_pressed.push(false);
            self.state.gamepad.buttons_released.push(false);
        }
        if pressed && !self.state.gamepad.buttons_down[idx] {
            self.state.gamepad.buttons_pressed[idx] = true;
        }
        self.state.gamepad.buttons_down[idx] = pressed;
        if !pressed {
            self.state.gamepad.buttons_released[idx] = true;
        }
    }

    /// Process a gamepad axis
    pub fn on_gamepad_axis(&mut self, axis_idx: u32, value: f32) {
        let idx = axis_idx as usize;
        while self.state.gamepad.axes.len() <= idx {
            self.state.gamepad.axes.push(0.0);
        }
        self.state.gamepad.axes[idx] = value;
    }

    /// End of frame -- clear frame-specific state
    pub fn end_frame(&mut self) {
        self.state.clear_frame();
    }

    /// Get the current input state (read-only reference)
    pub fn state(&self) -> &InputState { &self.state }
}
