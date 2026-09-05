# FidelityFX Integration

Litt Engine integrates the [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) for upscaling and frame generation.

> **Note:** The official FSR SDK is **DX12 only**. [OptiScaler](https://github.com/OptiScaler/OptiScaler) provides DX12<->Vulkan interop for FSR 4 runtime injection.

## FSR Versions

| Version | Type | GPUs | Description |
|---------|------|------|-------------|
| **FSR 1** | Spatial | All | Basic upscaling, no temporal |
| **FSR 2/3.1.5** | Temporal + Frame Gen | AMD, Intel, Samsung | AI-quality upscaling + FG |
| **FSR 3** | AI + Temporal + FG | RDNA 4/5 + all | Next-gen with ML reconstruction |

## GPU Support Matrix

| GPU | FSR 1 | FSR 3.1.5 | FSR 3 |
|-----|-------|-----------|-------|
| AMD RDNA 2 | Yes | Yes | No |
| AMD RDNA 3 | Yes | Yes | Yes |
| AMD RDNA 4 | Yes | Yes | Yes (Native) |
| Intel Arc | Yes | Yes | Yes |
| Samsung Exynos | Yes | Yes | Yes |
| Moore Threads | Yes | Yes | Partial |
| Qualcomm Adreno | Yes | Yes | Yes |
| MediaTek | Yes | Yes | Yes |
| Huawei Kirin | Yes | Yes | Yes |
| NVIDIA | Yes | Yes | Yes |

## Usage

```cpp
use litt_fidelityfx::fsr3::*;

let mut fsr3 = Fsr4::new(960, 540, 1920, 1080);
fsr3.support_level = Fsr4::detect_support(&device, physical_device);

match fsr3.support_level {
    Fsr4Support::Full => {
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::Full, true, true);
    }
    Fsr4Support::Temporal => {
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::FrameGen, false, true);
    }
    Fsr4Support::Spatial => {
        fsr3.update(Fsr4Quality::Quality, Fsr4Mode::Upscale, false, false);
    }
    _ => {}
}
```

## Quality Presets

| Preset | Resolution Scale | Use Case |
|--------|-----------------|----------|
| UltraQuality | 0.56x | Cinematic, AAA |
| Quality | 0.67x | Balanced |
| Balanced | 0.83x | Performance |
| Performance | 1.0x | Max FPS |
| UltraPerformance | 1.5x | Lowest res |

## Other FidelityFX Effects

| Effect | Description | Shader |
|--------|-------------|--------|
| **CAS** | Contrast Adaptive Sharpening | `cas.comp.glsl` |
| **Ray Reconstruction** | CNN-style denoiser | `ray_reconstruction.comp.glsl` |
| **Diffuse Denoiser** | Temporal-spatial diffuse | `denoiser_diffuse.comp.glsl` |
| **Specular Denoiser** | Temporal-spatial specular | `denoiser_specular.comp.glsl` |
| **XESS 3** | Intel frame generation | `xess3_framegen.comp.glsl` |

## Environment Variables

```bash
export LIT_FSR_MODE=4          # Use FSR 3 if available
export LIT_FSR_QUALITY=1       # Quality preset
export LIT_FSR_FRAMEGEN=1      # Enable frame generation
```

## Frame Graph Integration

```
Path Trace (Compute Shader)
  -> FidelityFX Ray Reconstruction (Denoiser)
  -> FidelityFX FSR 3.1.5
       Create Pass (temporal accumulation)
       Compensate Pass (motion vectors)
       Upscaler Pass (upscaling)
       Frame Gen Pass (frame generation)
  -> FidelityFX CAS (sharpening)
  -> Tonemap
  -> Present
```

See [../rendering/frame-graph.md](../rendering/frame-graph.md) for the full frame graph.

