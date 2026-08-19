# AMD FidelityFX Super Resolution (FSR) Support

Litt Engine integrates the [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) for upscaling and frame generation.

> **Note:** The official FSR SDK is **DX12 only**. [OptiScaler](https://github.com/OptiScaler/OptiScaler) provides DX12↔Vulkan interop for FSR 4 runtime injection. Litt Engine is compatible with OptiScaler's injection framework.

## FSR Versions

| Version | Type | GPUs | Description |
|---------|------|------|-------------|
| **FSR 1** | Spatial | All | Basic upscaling, no temporal |
| **FSR 2/3.1.5** | Temporal + Frame Gen | AMD, Intel, Samsung | AI-quality upscaling + FG |
| **FSR 3** | AI + Temporal + FG | RDNA 4/5 + all | Next-gen with ML reconstruction |

## GPU Support Matrix

| GPU | FSR 1 | FSR 3.1.5 | FSR 3 |
|-----|-------|-----------|-------|
| AMD RDNA 2 | ✅ | ✅ | ❌ (FSR 4.1 early 2027) |
| AMD RDNA 3 (desktop) | ✅ | ✅ | ✅ |
| AMD RDNA 4 | ✅ | ✅ | ✅ |
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
use litt_fidelityfx::fsr3::*;

// Initialize FSR 3
let mut fsr3 = Fsr4::new(960, 540, 1920, 1080);

// Detect GPU support level
fsr3.support_level = Fsr4::detect_support(&device, physical_device);

// Configure based on support
match fsr3.support_level {
    Fsr4Support::Full => {
        // RDNA 4/5: use full FSR 3 with AI reconstruction
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::Full, true, true);
    }
    Fsr4Support::Temporal => {
        // All other GPUs: use FSR 3.1.5 temporal upscaling
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::FrameGen, false, true);
    }
    Fsr4Support::Spatial => {
        // Basic GPUs: spatial upscaling only
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::Upscale, false, false);
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
export LIT_FSR_MODE=4          # Use FSR 3 if available
export LIT_FSR_QUALITY=1       # Quality preset
export LIT_FSR_FRAMEGEN=1      # Enable frame generation
\`\`\
