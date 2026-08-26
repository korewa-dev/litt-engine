# Dither3D Integration

> Surface-Stable Fractal Dithering for Litt Engine.
> Ported from [Dither3D](https://github.com/runevision/Dither3D) (MPL v2.0, Rune Skovbo Johansen).

## What is Dither3D?

Surface-Stable Fractal Dithering creates dither patterns that **stick to surfaces** while maintaining approximately constant dot size on screen — even as surfaces move closer or further away. This is achieved by dynamically adding or removing dots via fractal pattern interpolation.

### Key Properties

| Property | Description |
|----------|-------------|
| **Surface-stable** | Dots adhere to geometry, not screen space |
| **Fractal scaling** | Pattern density adapts to surface frequency |
| **Multi-mode** | Grayscale, RGB, CMYK halftone supported |
| **Configurable** | Scale, contrast, size variability, stretch |

## Shader Files

| File | Purpose |
|------|---------|
| `shaders/dither3d/include.glsl` | Core algorithm (ported from `Dither3DInclude.cginc`) |
| `shaders/dither3d/mesh.vert.glsl` | Vertex shader (passes screen position) |
| `shaders/dither3d/mesh.frag.glsl` | Fragment shader (applies dithering to albedo) |

## Algorithm Overview

```
Input: UV coordinates, screen position, albedo color
  |
  v
1. Compute UV derivatives (dFdx/dFdy)
  |
  v
2. SVD of Jacobian → min/max spatial frequency
  |
  v
3. Map frequency to dot spacing (surface-stable)
  |
  v
4. Lookup fractal pattern from 3D texture
  |
  v
5. Adjust dot size/contrast based on brightness
  |
  v
Output: Dithered color (grayscale / RGB / CMYK)
```

## GLSL Adaptations from Unity CG

| Unity CG | GLSL (Vulkan) | Notes |
|----------|---------------|-------|
| `ddx(uv)` | `dFdx(uv)` | Standard GLSL derivative |
| `ddy(uv)` | `dFdy(uv)` | Standard GLSL derivative |
| `determinant(mat)` | Manual: `a*d - b*c` | 2x2 determinant |
| `_DitherTex_TexelSize` | `uDitherTexWidth` uniform | Width passed from host |
| `UNITY_MATRIX_P[0][0]` | `uProj[0][0]` uniform | Projection matrix |
| `tex2D(sampler, uv)` | `texture(sampler, uv)` | Standard GLSL |
| `tex3D(sampler, uvw)` | `texture(sampler, uvw)` | Standard GLSL |
| `pow(B,-A)` | `pow(B,-A)` | Same |
| `saturate(x)` | `clamp(x, 0.0, 1.0)` | Standard GLSL |

## Dither Parameters

All parameters are passed via the `Constants` uniform block in `mesh.frag.glsl`:

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `uDitherEnabled` | float | 0.0 | 0-1 | Enable/disable dithering |
| `uDitherScale` | float | 5.0 | 2-10 | Exponential dot scale |
| `uDitherSizeVar` | float | 0.0 | 0-1 | 0=Bayer, 1=Halftone |
| `uDitherContrast` | float | 1.0 | 0-2 | Dot edge sharpness |
| `uDitherColorMode` | uint | 0 | 0-2 | 0=Gray, 1=RGB, 2=CMYK |
| `uDitherPattern` | uint | 3 | 0-3 | 0=1x1, 1=2x2, 2=4x4, 3=8x8 |
| `uDitherInputExp` | float | 1.0 | 0-5 | Brightness exposure |
| `uDitherInputOff` | float | 0.0 | -1-1 | Brightness offset |

## Texture Requirements

The 3D dither texture must be created from the Dither3D source PNGs:

| Pattern | Source PNG | Texture Dim | Layers | Size |
|---------|-----------|-------------|--------|------|
| 1x1 | `Dither3D_1x1.png` | 16x16 | 1 | 256 bytes |
| 2x2 | `Dither3D_2x2.png` | 32x32 | 4 | 4 KB |
| 4x4 | `Dither3D_4x4.png` | 64x64 | 16 | 64 KB |
| **8x8** | `Dither3D_8x8.png` | **128x128** | **64** | **1 MB** |

The 8x8 pattern is the default and highest quality. Create as `GL_TEXTURE_3D` with `GL_R8` format.

The ramp texture is a 1D lookup:
| File | Dimensions | Format |
|------|-----------|--------|
| `Dither3D_8x8_Ramp.png` | 256x1 | `GL_R8` |

## Compile-Time Features

Set via `#define` in the fragment shader:

| Feature | Define | Effect |
|---------|--------|--------|
| Inverse dots | `#define DITHER_INVERSE` | Swap light/dark |
| Radial compensation | `#define DITHER_RADIAL` | Screen-edge stability |
| Quantize layers | `#define DITHER_QUANTIZE` | No fractal interpolation |
| Debug fractal | `#define DITHER_DEBUG` | Visualize pattern level |

## ECS Integration

Add dithering support to the material system:

```rust
// In materials.rs or a new dither module
#[derive(Component, Debug, Clone)]
pub struct DitherMaterial {
    pub enabled: bool,
    pub scale: f32,
    pub size_variability: f32,
    pub contrast: f32,
    pub color_mode: DitherColorMode,
    pub pattern: DitherPattern,
    pub input_exposure: f32,
    pub input_offset: f32,
}

pub enum DitherColorMode {
    Grayscale,
    RGB,
    CMYK,
}

pub enum DitherPattern {
    Pattern1x1,
    Pattern2x2,
    Pattern4x4,
    Pattern8x8,
}
```

## Frame Graph Integration

Dither3D is applied as a **post-mesh, pre-tonemap** step. Two options:

### Option A: Per-pixel in mesh fragment (current implementation)
```
Opaque Render → Dither (in mesh.frag) → UI Overlay → Post-Process → Present
```
- Simplest integration
- Works per-material
- Dithering is part of the main render pass

### Option B: Dedicated compute pass (future)
```
Opaque Render → UI Overlay → Dither Compute → Tonemap → Post-Process → Present
```
- More flexible (can dither path tracer output too)
- Requires additional render target
- Better for path tracing workflows

## Build.rs Integration

No changes needed — the existing glob pattern `shaders/**/*.glsl` will automatically pick up the new shader files.

## License

Dither3D is licensed under **MPL v2.0**. The ported GLSL code retains this license. See [LICENSE.md](https://github.com/runevision/Dither3D/blob/main/LICENSE.md) for details.
