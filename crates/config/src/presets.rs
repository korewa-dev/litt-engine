//! Graphics presets — pre-configured settings for different hardware tiers.

use super::settings::*;

/// Get settings for a quality preset
pub fn get_preset(preset: &str) -> Settings {
    match preset {
        "low" => Settings {
            graphics_quality: GraphicsQuality::Low,
            aa_mode: AAMode::Off,
            shadow_quality: ShadowQuality::Low,
            ray_tracing: false,
            texture_quality: TextureQuality::Low,
            post_processing: false,
            fsr_mode: FSRMode::UltraPerformance,
            max_fps: 60,
            ..Settings::default()
        },
        "medium" => Settings {
            graphics_quality: GraphicsQuality::Medium,
            aa_mode: AAMode::FXAA,
            shadow_quality: ShadowQuality::Medium,
            ray_tracing: true,
            texture_quality: TextureQuality::Medium,
            post_processing: true,
            fsr_mode: FSRMode::Performance,
            max_fps: 60,
            ..Settings::default()
        },
        "high" => Settings {
            graphics_quality: GraphicsQuality::High,
            aa_mode: AAMode::TAA,
            shadow_quality: ShadowQuality::High,
            ray_tracing: true,
            texture_quality: TextureQuality::High,
            post_processing: true,
            fsr_mode: FSRMode::Quality,
            max_fps: 144,
            ..Settings::default()
        },
        "ultra" => Settings {
            graphics_quality: GraphicsQuality::Ultra,
            aa_mode: AAMode::MSAA4x,
            shadow_quality: ShadowQuality::Ultra,
            ray_tracing: true,
            texture_quality: TextureQuality::Ultra,
            post_processing: true,
            fsr_mode: FSRMode::Off,
            max_fps: 240,
            ..Settings::default()
        },
        _ => Settings::default(),
    }
}
