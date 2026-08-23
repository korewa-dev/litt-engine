//! Keyboard input handling.
//! Platform-specific key codes mapped to virtual keys.

/// Virtual key codes (cross-platform)
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Key {
    // Alpha
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Navigation
    Escape, Return, Tab, Backspace, Space,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    Insert, Delete, Home, End, PageUp, PageDown,
    // Function
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // Symbols
    Grave, Minus, Equals, BracketLeft, BracketRight, Backslash, Semicolon, Quote, Comma, Period, Slash,
    // Control
    CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadDecimal, NumpadEnter,
    // Media
    VolumeUp, VolumeDown, VolumeMute,
    // Modifiers
    LShift, RShift, LControl, RControl, LAlt, RAlt,
    // Misc
    Menu,
}

impl Key {
    /// Check if key is pressed
    pub fn is_special(&self) -> bool {
        matches!(self,
            Self::Escape | Self::Return | Self::Tab | Self::Backspace | Self::Space
            | Self::ArrowUp | Self::ArrowDown | Self::ArrowLeft | Self::ArrowRight
            | Self::Insert | Self::Delete | Self::Home | Self::End
            | Self::PageUp | Self::PageDown
            | Self::F1 | Self::F2 | Self::F3 | Self::F4 | Self::F5 | Self::F6
            | Self::F7 | Self::F8 | Self::F9 | Self::F10 | Self::F11 | Self::F12
            | Self::CapsLock | Self::ScrollLock | Self::NumLock
            | Self::PrintScreen | Self::Pause
        )
    }
}

impl Default for Key {
    fn default() -> Self { Self::A }
}

/// Keyboard state
#[derive(Clone, Debug, Default)]
pub struct KeyboardState {
    pub(crate) keys_down: Vec<Key>,
    pub(crate) keys_pressed: Vec<Key>,
    pub(crate) keys_released: Vec<Key>,
}

impl KeyboardState {
    pub fn new() -> Self { Self::default() }

    pub fn is_down(&self, key: Key) -> bool {
        self.keys_down.contains(&key)
    }

    pub fn is_pressed(&self, key: Key) -> bool {
        self.keys_pressed.contains(&key)
    }

    pub fn is_released(&self, key: Key) -> bool {
        self.keys_released.contains(&key)
    }

    pub fn clear_frame(&mut self) {
        self.keys_pressed.clear();
        self.keys_released.clear();
    }
}

/// Convert virtual key to platform-specific scancode
pub fn key_to_scancode(key: Key) -> u32 {
    // Standard scancode mapping
    match key {
        Key::A => 0x04, Key::B => 0x05, Key::C => 0x06, Key::D => 0x07,
        Key::E => 0x08, Key::F => 0x09, Key::G => 0x0A, Key::H => 0x0B,
        Key::I => 0x0C, Key::J => 0x0D, Key::K => 0x0E, Key::L => 0x0F,
        Key::M => 0x10, Key::N => 0x11, Key::O => 0x12, Key::P => 0x13,
        Key::Q => 0x14, Key::R => 0x15, Key::S => 0x16, Key::T => 0x17,
        Key::U => 0x18, Key::V => 0x19, Key::W => 0x1A, Key::X => 0x1B,
        Key::Y => 0x1C, Key::Z => 0x1D,
        Key::Num1 => 0x02, Key::Num2 => 0x03, Key::Num3 => 0x04,
        Key::Num4 => 0x05, Key::Num5 => 0x06, Key::Num6 => 0x07,
        Key::Num7 => 0x08, Key::Num8 => 0x09, Key::Num9 => 0x0A,
        Key::Num0 => 0x0B,
        Key::LShift => 0xE1, Key::RShift => 0xE5, Key::LControl => 0xE0,
        Key::RControl => 0xE4, Key::LAlt => 0xE2, Key::RAlt => 0xE6,
        Key::Return => 0x28, Key::Escape => 0x01, Key::Backspace => 0x0E,
        Key::Tab => 0x0F, Key::Space => 0x39,
        Key::ArrowUp => 0xC8, Key::ArrowDown => 0xD0,
        Key::ArrowLeft => 0xCB, Key::ArrowRight => 0xCD,
        _ => 0,
    }
}

