//! Audio player -- plays sounds with volume, pitch, and spatial positioning.

use super::sound::Sound;

/// Audio source type
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SourceType {
    /// One-shot sound effect
    OneShot,
    /// Looping ambient sound
    Loop,
    /// Music track
    Music,
}

/// An audio source
#[derive(Debug)]
pub struct AudioSource {
    pub sound: Sound,
    pub source_type: SourceType,
    pub volume: f32,
    pub pitch: f32,
    pub position: (f32, f32, f32),
    pub playing: bool,
    pub current_time: f32,
}

impl AudioSource {
    pub fn new(sound: Sound, source_type: SourceType) -> Self {
        Self {
            sound,
            source_type,
            volume: 1.0,
            pitch: 1.0,
            position: (0.0, 0.0, 0.0),
            playing: false,
            current_time: 0.0,
        }
    }

    pub fn play(&mut self) {
        self.playing = true;
        self.current_time = 0.0;
    }

    pub fn pause(&mut self) {
        self.playing = false;
    }

    pub fn stop(&mut self) {
        self.playing = false;
        self.current_time = 0.0;
    }

    pub fn is_playing(&self) -> bool {
        self.playing
    }
}
