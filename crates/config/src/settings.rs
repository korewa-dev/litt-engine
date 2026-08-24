//! Engine settings -- graphics, audio, input, and performance configuration.
//!
//! Settings serialize to plain JSON (`litt_engine.json`) so both humans and
//! AI agents can inspect or rewrite them with text tools.

use serde::{Deserialize, Serialize};

/// Graphics quality preset
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum GraphicsQuality {
    Low,
    Medium,
    High,
    Ultra,
    Custom,
}

/// Anti-aliasing mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum AAMode {
    Off,
    FXAA,
    TAA,
    MSAA2x,
    MSAA4x,
    MSAA8x,
}

/// Shadow quality
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum ShadowQuality {
    Off,
    Low,
    Medium,
    High,
    Ultra,
}

/// Texture quality
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum TextureQuality {
    Low,
    Medium,
    High,
    Ultra,
}

/// FSR mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum FSRMode {
    Off,
    Quality,
    Balanced,
    Performance,
    UltraPerformance,
}

/// Engine settings
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    // Graphics
    pub window_title: String,
    pub window_width: u32,
    pub window_height: u32,
    pub fullscreen: bool,
    pub vsync: bool,
    pub graphics_quality: GraphicsQuality,
    pub aa_mode: AAMode,
    pub shadow_quality: ShadowQuality,
    pub ray_tracing: bool,
    pub texture_quality: TextureQuality,
    pub post_processing: bool,
    pub fsr_mode: FSRMode,
    pub cas_enabled: bool,

    // Audio
    pub master_volume: f32,
    pub music_volume: f32,
    pub sfx_volume: f32,
    pub audio_enabled: bool,

    // Input
    pub mouse_sensitivity: f32,
    pub invert_y: bool,
    pub hold_to_run: bool,

    // Performance
    pub max_fps: u32,
    pub target_frame_time_ms: f32,
    pub enable_profiler: bool,
    pub enable_debug_overlay: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            window_title: "Litt Engine".to_string(),
            window_width: 1280,
            window_height: 720,
            fullscreen: false,
            vsync: true,
            graphics_quality: GraphicsQuality::High,
            aa_mode: AAMode::TAA,
            shadow_quality: ShadowQuality::High,
            ray_tracing: true,
            texture_quality: TextureQuality::High,
            post_processing: true,
            fsr_mode: FSRMode::Quality,
            cas_enabled: true,
            master_volume: 0.8,
            music_volume: 0.6,
            sfx_volume: 0.8,
            audio_enabled: true,
            mouse_sensitivity: 0.002,
            invert_y: false,
            hold_to_run: false,
            max_fps: 144,
            target_frame_time_ms: 8.33,
            enable_profiler: false,
            enable_debug_overlay: false,
        }
    }
}

impl Settings {
    /// Clamp user-adjustable values into safe ranges (menu sliders call this).
    pub fn sanitize(&mut self) {
        self.window_width = self.window_width.clamp(320, 7680);
        self.window_height = self.window_height.clamp(200, 4320);
        self.master_volume = self.master_volume.clamp(0.0, 1.0);
        self.music_volume = self.music_volume.clamp(0.0, 1.0);
        self.sfx_volume = self.sfx_volume.clamp(0.0, 1.0);
        self.mouse_sensitivity = self.mouse_sensitivity.clamp(0.0002, 0.02);
        self.max_fps = self.max_fps.clamp(30, 1000);
    }
}

/// Save settings to JSON file
pub fn save_settings(settings: &Settings, path: &str) -> Result<(), String> {
    let json = serde_json::to_string_pretty(settings)
        .map_err(|e| format!("Failed to serialize settings: {}", e))?;
    std::fs::write(path, json).map_err(|e| format!("Failed to write settings: {}", e))?;
    Ok(())
}

/// Load settings from JSON file
pub fn load_settings(path: &str) -> Result<Settings, String> {
    let data = std::fs::read_to_string(path)
        .map_err(|e| format!("Failed to read settings: {}", e))?;
    let settings: Settings =
        serde_json::from_str(&data).map_err(|e| format!("Failed to parse settings: {}", e))?;
    Ok(settings)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn settings_roundtrip_json() {
        let mut s = Settings::default();
        s.master_volume = 0.42;
        s.aa_mode = AAMode::MSAA4x;
        s.fsr_mode = FSRMode::UltraPerformance;
        s.fullscreen = true;

        let json = serde_json::to_string(&s).unwrap();
        let back: Settings = serde_json::from_str(&json).unwrap();
        assert_eq!(back.master_volume, 0.42);
        assert_eq!(back.aa_mode, AAMode::MSAA4x);
        assert_eq!(back.fsr_mode, FSRMode::UltraPerformance);
        assert!(back.fullscreen);
    }

    #[test]
    fn settings_file_roundtrip() {
        let dir = std::env::temp_dir().join("litt_config_test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("settings.json");
        let path_str = path.to_str().unwrap();

        let mut s = Settings::default();
        s.mouse_sensitivity = 0.005;
        save_settings(&s, path_str).unwrap();
        let back = load_settings(path_str).unwrap();
        assert_eq!(back.mouse_sensitivity, 0.005);
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn partial_json_fills_defaults() {
        // Older/handwritten config files may lack new fields -- they must
        // still load thanks to #[serde(default)].
        let back: Settings = serde_json::from_str("{\"window_width\": 800}").unwrap();
        assert_eq!(back.window_width, 800);
        assert_eq!(back.window_height, 720);
        assert!(back.vsync);
    }

    #[test]
    fn sanitize_clamps_ranges() {
        let mut s = Settings::default();
        s.master_volume = 5.0;
        s.max_fps = 10_000;
        s.mouse_sensitivity = 1.0;
        s.sanitize();
        assert_eq!(s.master_volume, 1.0);
        assert_eq!(s.max_fps, 1000);
        assert!(s.mouse_sensitivity <= 0.02);
    }
}
