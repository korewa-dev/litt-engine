//! Audio context -- manages audio devices and plays sources.
//! Uses cpal for cross-platform audio output.

use super::audio_player::AudioSource;
use super::sound::Sound;

/// Audio context for managing playback
#[derive(Debug)]
pub struct AudioContext {
    pub sources: Vec<AudioSource>,
    pub master_volume: f32,
    pub sample_rate: u32,
    pub initialized: bool,
}

impl Default for AudioContext {
    fn default() -> Self {
        Self::new()
    }
}

impl AudioContext {
    /// Create a new audio context
    pub fn new() -> Self {
        Self {
            sources: Vec::new(),
            master_volume: 1.0,
            sample_rate: 44100,
            initialized: cfg!(feature = "cpal"),
        }
    }

    /// Initialize audio backend
    pub fn init(&mut self) -> Result<(), String> {
        #[cfg(feature = "cpal")]
        {
            // cpal would be initialized here
            self.initialized = true;
        }
        Ok(())
    }

    /// Play a sound
    pub fn play(&mut self, sound: Sound, source_type: super::audio_player::SourceType) -> Option<usize> {
        let mut source = AudioSource::new(sound, source_type);
        source.play();
        let idx = self.sources.len();
        self.sources.push(source);
        Some(idx)
    }

    /// Stop a sound by index
    pub fn stop(&mut self, idx: usize) {
        if let Some(source) = self.sources.get_mut(idx) {
            source.stop();
        }
    }

    /// Remove stopped sources
    pub fn cleanup(&mut self) {
        self.sources.retain(|s| s.is_playing());
    }

    /// Update audio sources (mixing)
    pub fn update(&mut self, dt: f32) {
        for source in &mut self.sources {
            if source.playing {
                source.current_time += dt * source.pitch;
                if source.current_time >= source.sound.duration_sec {
                    if source.source_type == super::audio_player::SourceType::OneShot {
                        source.stop();
                    } else {
                        source.current_time = 0.0;
                    }
                }
            }
        }
        self.cleanup();
    }
}
