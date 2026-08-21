//! Engine settings — graphics, audio, input, and performance configuration.

use litt_math::Vec2;

/// Graphics quality preset
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GraphicsQuality {
    Low,
    Medium,
    High,
    Ultra,
    Custom,
}

/// Anti-aliasing mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AAMode {
    Off,
    FXAA,
    TAA,
    MSAA2x,
    MSAA4x,
    MSAA8x,
}

/// Shadow quality
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShadowQuality {
    Off,
    Low,
    Medium,
    High,
    Ultra,
}

/// Engine settings
#[derive(Clone, Debug)]
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

/// Texture quality
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TextureQuality {
    Low,
    Medium,
    High,
    Ultra,
}

/// FSR mode
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FSRMode {
    Off,
    Quality,
    Balanced,
    Performance,
    UltraPerformance,
}

/// Save settings to JSON file
pub fn save_settings(settings: &Settings, path: &str) -> Result<(), String> {
    #[cfg(feature = "serde")]
    {
        let json = serde_json::to_string_pretty(settings)
            .map_err(|e| format!("Failed to serialize settings: {}", e))?;
        std::fs::write(path, json)
            .map_err(|e| format!("Failed to write settings: {}", e))?;
        Ok(())
    }
    #[cfg(not(feature = "serde"))]
    {
        let _ = settings;
        let _ = path;
        Err("serde feature not enabled".to_string())
    }
}

/// Load settings from JSON file
pub fn load_settings(path: &str) -> Result<Settings, String> {
    #[cfg(feature = "serde")]
    {
        let data = std::fs::read_to_string(path)
            .map_err(|e| format!("Failed to read settings: {}", e))?;
        let settings: Settings = serde_json::from_str(&data)
            .map_err(|e| format!("Failed to parse settings: {}", e))?;
        Ok(settings)
    }
    #[cfg(not(feature = "serde"))]
    {
        let _ = path;
        Err("serde feature not enabled".to_string())
    }
}
