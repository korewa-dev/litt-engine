//! Config manager -- loads, saves, and applies settings.

use super::settings::Settings;
use super::presets::get_preset;

/// Config manager
#[derive(Debug)]
pub struct ConfigManager {
    pub settings: Settings,
    pub current_preset: String,
    pub config_path: String,
}

impl Default for ConfigManager {
    fn default() -> Self { Self::new() }
}

impl ConfigManager {
    /// Create a new config manager
    pub fn new() -> Self {
        Self {
            settings: Settings::default(),
            current_preset: "high".to_string(),
            config_path: "litt_engine.json".to_string(),
        }
    }

    /// Apply a preset
    pub fn apply_preset(&mut self, preset: &str) {
        self.settings = get_preset(preset);
        self.current_preset = preset.to_string();
    }

    /// Load config from file
    pub fn load(&mut self) -> Result<(), String> {
        match super::settings::load_settings(&self.config_path) {
            Ok(settings) => {
                self.settings = settings;
                Ok(())
            }
            Err(_) => {
                // No config file, use defaults
                Ok(())
            }
        }
    }

    /// Save config to file
    pub fn save(&self) -> Result<(), String> {
        super::settings::save_settings(&self.settings, &self.config_path)
    }

    /// Get window size
    pub fn window_size(&self) -> (u32, u32) {
        (self.settings.window_width, self.settings.window_height)
    }

    /// Check if fullscreen
    pub fn is_fullscreen(&self) -> bool {
        self.settings.fullscreen
    }

    /// Check if ray tracing is enabled
    pub fn ray_tracing_enabled(&self) -> bool {
        self.settings.ray_tracing
    }

    /// Check if debug overlay is enabled
    pub fn debug_overlay_enabled(&self) -> bool {
        self.settings.enable_debug_overlay
    }
}
