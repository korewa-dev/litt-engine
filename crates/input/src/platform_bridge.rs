//! Platform event bridge -- native virtual-key codes to engine `Key`s.
//!
//! `litt-platform` queues raw OS events; this module translates them and
//! feeds the [`InputSystem`], so game code only ever sees logical keys.

use crate::input_system::InputSystem;
use crate::keyboard::Key;
use litt_platform::PlatformEvent;

/// Map a native Windows virtual-key code to an engine key.
///
/// Codes follow the Win32 `VK_*` constants (0x00-0xFF). Unknown codes
/// return `None` and are safely ignored by [`InputSystem::ingest_platform`].
pub fn vk_to_key(vk: u32) -> Option<Key> {
    let k = match vk {
        // Letters A-Z are contiguous 0x41..=0x5A.
        0x41..=0x5A => return Some(letter(vk - 0x41)),
        // Digits 0-9 are contiguous 0x30..=0x39.
        0x30..=0x39 => return Some(digit(vk - 0x30)),

        0x08 => Key::Backspace,
        0x09 => Key::Tab,
        0x0D => Key::Return,
        0x13 => Key::Pause,
        0x14 => Key::CapsLock,
        0x1B => Key::Escape,
        0x20 => Key::Space,

        0x21 => Key::PageUp,
        0x22 => Key::PageDown,
        0x23 => Key::End,
        0x24 => Key::Home,
        0x25 => Key::ArrowLeft,
        0x26 => Key::ArrowUp,
        0x27 => Key::ArrowRight,
        0x28 => Key::ArrowDown,
        0x2D => Key::Insert,
        0x2E => Key::Delete,

        0x60..=0x69 => return Some(numpad(vk - 0x60)),
        0x6A => Key::NumpadMultiply,
        0x6B => Key::NumpadAdd,
        0x6D => Key::NumpadSubtract,
        0x6E => Key::NumpadDecimal,
        0x6F => Key::NumpadDivide,

        0x70..=0x7B => return Some(function(vk - 0x70)),

        0xA0 | 0x10 => Key::LShift,
        0xA1 => Key::RShift,
        0xA2 | 0x11 => Key::LControl,
        0xA3 => Key::RControl,
        0xA4 | 0x12 => Key::LAlt,
        0xA5 => Key::RAlt,

        0x90 => Key::NumLock,
        0x91 => Key::ScrollLock,
        0x2C => Key::PrintScreen,

        0xAD => Key::VolumeMute,
        0xAE => Key::VolumeDown,
        0xAF => Key::VolumeUp,

        _ => return None,
    };
    Some(k)
}

fn letter(offset: u32) -> Key {
    const LETTERS: [Key; 26] = [
        Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H, Key::I, Key::J, Key::K,
        Key::L, Key::M, Key::N, Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T, Key::U, Key::V,
        Key::W, Key::X, Key::Y, Key::Z,
    ];
    LETTERS[offset as usize]
}

fn digit(offset: u32) -> Key {
    const DIGITS: [Key; 10] = [
        Key::Num0, Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5, Key::Num6, Key::Num7,
        Key::Num8, Key::Num9,
    ];
    DIGITS[offset as usize]
}

fn numpad(offset: u32) -> Key {
    const KEYS: [Key; 10] = [
        Key::Numpad0, Key::Numpad1, Key::Numpad2, Key::Numpad3, Key::Numpad4, Key::Numpad5,
        Key::Numpad6, Key::Numpad7, Key::Numpad8, Key::Numpad9,
    ];
    KEYS[offset as usize]
}

fn function(offset: u32) -> Key {
    const FKEYS: [Key; 12] = [
        Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9,
        Key::F10, Key::F11, Key::F12,
    ];
    FKEYS[offset as usize]
}

impl InputSystem {
    /// Feed raw platform events (from `Window::take_events`) into input state.
    pub fn ingest_platform(&mut self, events: &[PlatformEvent]) {
        for event in events {
            match *event {
                PlatformEvent::KeyDown { vk } => {
                    if let Some(k) = vk_to_key(vk) {
                        self.on_key_down(k);
                    }
                }
                PlatformEvent::KeyUp { vk } => {
                    if let Some(k) = vk_to_key(vk) {
                        self.on_key_up(k);
                    }
                }
                // Char/Resize/CloseRequested belong to text/UI/window layers.
                PlatformEvent::Char(_) | PlatformEvent::CloseRequested | PlatformEvent::Resize { .. } => {}
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_core_navigation_keys() {
        assert_eq!(vk_to_key(0x1B), Some(Key::Escape));
        assert_eq!(vk_to_key(0x0D), Some(Key::Return));
        assert_eq!(vk_to_key(0x26), Some(Key::ArrowUp));
        assert_eq!(vk_to_key(0x28), Some(Key::ArrowDown));
        assert_eq!(vk_to_key(0x25), Some(Key::ArrowLeft));
        assert_eq!(vk_to_key(0x27), Some(Key::ArrowRight));
        assert_eq!(vk_to_key(0x74), Some(Key::F5));
        assert_eq!(vk_to_key(0x11), Some(Key::LControl));
    }

    #[test]
    fn maps_letters_digits_contiguously() {
        assert_eq!(vk_to_key(0x41), Some(Key::A));
        assert_eq!(vk_to_key(0x5A), Some(Key::Z));
        assert_eq!(vk_to_key(0x44), Some(Key::D));
        assert_eq!(vk_to_key(0x31), Some(Key::Num1));
        assert_eq!(vk_to_key(0x39), Some(Key::Num9));
    }

    #[test]
    fn unknown_codes_are_none() {
        assert_eq!(vk_to_key(0x00), None);
        assert_eq!(vk_to_key(0xC0), None); // OEM keys not mapped yet
        assert_eq!(vk_to_key(u32::MAX), None);
    }

    #[test]
    fn ingest_drives_pressed_state() {
        let mut input = InputSystem::new();
        input.ingest_platform(&[
            PlatformEvent::KeyDown { vk: 0x1B },                       // Escape down
            PlatformEvent::KeyDown { vk: 0x1B },                       // duplicate ignored
            PlatformEvent::KeyDown { vk: 0xC0 },                       // unknown, skipped
        ]);
        assert!(input.state().key_pressed(Key::Escape));
        // Exactly one entry despite duplicate event.
        assert_eq!(input.state.keyboard.keys_down.iter().filter(|&&k| k == Key::Escape).count(), 1);

        input.end_frame();
        input.ingest_platform(&[PlatformEvent::KeyUp { vk: 0x1B }]);
        assert!(!input.state().key_pressed(Key::Escape));
        assert!(!input.state.keyboard.keys_down.contains(&Key::Escape));
    }
}
