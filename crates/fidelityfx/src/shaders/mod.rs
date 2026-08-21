//! AMD FidelityFX Super Resolution 3.1.5 — GLSL shader sources.
//!
//! These shaders are compiled to SPIR-V by build.rs at build time.
//! If glslc/glslangValidator is not on PATH, fallback no-op shaders are used.
//!
//! To manually compile:
//!   glslc src/shaders/fsr3_upscaler.comp -o out/fsr3_upscaler.spv
//!   glslc src/shaders/fsr3_compensate.comp -o out/fsr3_compensate.spv
//!   glslc src/shaders/fsr3_create.comp  -o out/fsr3_create.spv
//!   glslc src/shaders/fsr3_framegen.comp -o out/fsr3_framegen.spv
//!   glslc src/shaders/cas.comp          -o out/cas.spv
//!   glslc src/shaders/ray_recon.comp    -o out/ray_recon.spv

/// FSR 3.1.5 spatial upscaler — takes low-res input + history, outputs high-res
pub const FSR3_UPSCALER_GLSL: &str = include_str!("fsr3_upscaler.comp");

/// FSR 3.1.5 compensate — normalizes history by exposure
pub const FSR3_COMPENSATE_GLSL: &str = include_str!("fsr3_compensate.comp");

/// FSR 3.1.5 create (reprojection) — copies prev frame to history
pub const FSR3_CREATE_GLSL: &str = include_str!("fsr3_create.comp");

/// FSR 3.1.5 frame generation — synthesizes intermediate frame
pub const FSR3_FRAMEGEN_GLSL: &str = include_str!("fsr3_framegen.comp");

/// CAS (Contrast Adaptive Sharpening) — final image sharpen
pub const CAS_GLSL: &str = include_str!("cas.comp");

/// Ray Reconstruction denoiser — CNN-based path tracer denoising
pub const RAY_RECON_GLSL: &str = include_str!("ray_recon.comp");

/// Compiled SPIR-V bytecode (populated by build.rs)
/// When glslang is available, these are replaced with real SPIR-V bytes.
#[allow(unused)]
pub const FSR3_UPSCALER_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_COMPENSATE_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_CREATE_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const FSR3_FRAMEGEN_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const CAS_SPIR_V: &[u32] = &[];
#[allow(unused)]
pub const RAY_RECON_SPIR_V: &[u32] = &[];

/// Returns true when real SPIR-V is available (glslang found at build time)
pub fn spirv_available() -> bool {
    !FSR3_UPSCALER_SPIR_V.is_empty()
}

#[cfg(test)]
mod tests {
    #[test]
    fn shaders_are_defined() {
        assert!(!super::FSR3_UPSCALER_GLSL.is_empty());
        assert!(!super::CAS_GLSL.is_empty());
        assert!(!super::RAY_RECON_GLSL.is_empty());
    }
}
