# AMD FidelityFX Super Resolution (FSR) Support

Litt Engine supports the full FSR family for upscaling and frame generation across all GPUs.

## FSR Versions

| Version | Type | GPUs | Description |
|---------|------|------|-------------|
| **FSR 1** | Spatial | All | Basic upscaling, no temporal |
| **FSR 2/3.1.5** | Temporal + Frame Gen | AMD, Intel, Samsung | AI-quality upscaling + FG |
| **FSR 4** | AI + Temporal + FG | RDNA 4/5 + all | Next-gen with ML reconstruction |

## GPU Support Matrix

| GPU | FSR 1 | FSR 3.1.5 | FSR 4 |
|-----|-------|-----------|-------|
| AMD RDNA 2 | ✅ | ✅ | Partial |
| AMD RDNA 3 | ✅ | ✅ | ✅ |
| AMD RDNA 4 | ✅ | ✅ | ✅ (Native) |
| Intel Arc | ✅ | ✅ | ✅ |
| Samsung Exynos | ✅ | ✅ | ✅ |
| Moore Threads | ✅ | ✅ | Partial |
| Qualcomm Adreno | ✅ | ✅ | ✅ |
| MediaTek | ✅ | ✅ | ✅ |
| Huawei Kirin | ✅ | ✅ | ✅ |
| NVIDIA | ✅ | ✅ | ✅ |

## Usage

\`\`\rust
use litt_fidelityfx::fsr4::*;

// Initialize FSR 4
let mut fsr4 = Fsr4::new(960, 540, 1920, 1080);

// Detect GPU support level
fsr4.support_level = Fsr4::detect_support(&device, physical_device);

// Configure based on support
match fsr4.support_level {
    Fsr4Support::Full => {
        // RDNA 4/5: use full FSR 4 with AI reconstruction
        fsr4.update(Fsr4Quality::Quality, Fsr4Mode::Full, true, true);
    }
    Fsr4Support::Temporal => {
        // All other GPUs: use FSR 3.1.5 temporal upscaling
        fsr4.update(Fsr4Quality::Quality, Fsr4Mode::FrameGen, false, true);
    }
    Fsr4Support::Spatial => {
        // Basic GPUs: spatial upscaling only
        fsr4.update(Fsr4Quality::Quality, Fsr4Mode::Upscale, false, false);
    }
    _ => {}
}
\`\`\

## Quality Presets

| Preset | Resolution Scale | Use Case |
|--------|-----------------|----------|
| UltraQuality | 0.56x | Cinematic, AAA |
| Quality | 0.67x | Balanced |
| Balanced | 0.83x | Performance |
| Performance | 1.0x | Max FPS |
| UltraPerformance | 1.5x | Lowest res |

## Environment Variables

\`\`\bash
# Force FSR mode
export LIT_FSR_MODE=4          # Use FSR 4 if available
export LIT_FSR_QUALITY=1       # Quality preset
export LIT_FSR_FRAMEGEN=1      # Enable frame generation
\`\`\
