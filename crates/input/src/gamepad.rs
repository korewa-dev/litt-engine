//! Gamepad input handling.
//! Supports Xbox, PlayStation, and generic gamepads.

use litt_math::Vec2;

/// Gamepad button
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum GamepadButton {
    // Face buttons
    A, B, X, Y,
    // Shoulder buttons
    LeftBumper, RightBumper,
    // Select/Start
    Back, Start,
    // Sticks
    LeftStick, RightStick,
    // D-pad
    DPadUp, DPadDown, DPadLeft, DPadRight,
    // Misc
    Home,
}

impl GamepadButton {
    pub fn index(&self) -> u32 {
        match self {
            Self::A => 0, Self::B => 1, Self::X => 2, Self::Y => 3,
            Self::LeftBumper => 4, Self::RightBumper => 5,
            Self::Back => 6, Self::Start => 7,
            Self::LeftStick => 8, Self::RightStick => 9,
            Self::DPadUp => 10, Self::DPadDown => 11, Self::DPadLeft => 12, Self::DPadRight => 13,
            Self::Home => 14,
        }
    }
}

/// Gamepad axis
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum GamepadAxis {
    LeftStickX, LeftStickY,
    RightStickX, RightStickY,
    LeftTrigger, RightTrigger,
}

impl GamepadAxis {
    pub fn index(&self) -> u32 {
        match self {
            Self::LeftStickX => 0, Self::LeftStickY => 1,
            Self::RightStickX => 2, Self::RightStickY => 3,
            Self::LeftTrigger => 4, Self::RightTrigger => 5,
        }
    }
}

/// Gamepad state
#[derive(Clone, Debug, Default)]
pub struct GamepadState {
    pub connected: bool,
    pub buttons_down: Vec<bool>,
    pub buttons_pressed: Vec<bool>,
    pub buttons_released: Vec<bool>,
    pub axes: Vec<f32>,
    pub vibration_left: f32,
    pub vibration_right: f32,
}

impl GamepadState {
    pub fn new() -> Self { Self::default() }

    pub fn is_down(&self, button: GamepadButton) -> bool {
        let idx = button.index() as usize;
        idx < self.buttons_down.len() && self.buttons_down[idx]
    }

    pub fn is_pressed(&self, button: GamepadButton) -> bool {
        let idx = button.index() as usize;
        idx < self.buttons_pressed.len() && self.buttons_pressed[idx]
    }

    pub fn axis(&self, axis: GamepadAxis) -> f32 {
        let idx = axis.index() as usize;
        if idx < self.axes.len() { self.axes[idx] } else { 0.0 }
    }

    pub fn clear_frame(&mut self) {
        self.buttons_pressed.fill(false);
        self.buttons_released.fill(false);
    }
}

/// Gamepad connection state
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GamepadConnection {
    Disconnected,
    Connected,
}
