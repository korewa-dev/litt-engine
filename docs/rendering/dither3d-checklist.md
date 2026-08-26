# Dither3D Integration Checklist

## Status: ✅ Complete

### Shaders
- [x] `shaders/dither3d/include.glsl` - Core algorithm (ported from Dither3DInclude.cginc)
- [x] `shaders/dither3d/mesh.vert.glsl` - Vertex shader (passes screen pos)
- [x] `shaders/dither3d/mesh.frag.glsl` - Fragment shader (applies dithering)

### C++ API
- [x] `native/littcore/litt_dither.h` - DitherMaterial, DitherAssetManager
- [x] `native/littcore/litt_dither.cpp` - Texture generation, PNG loading stub
- [x] `native/littcore/litt_dither_renderer.cpp` - Vulkan upload stub
- [x] `native/littcore/litt_renderer.h` - Added dither enable/disable methods

### Assets
- [x] `assets/dither3d/Dither3D_1x1.png`
- [x] `assets/dither3d/Dither3D_1x1_Ramp.png`
- [x] `assets/dither3d/Dither3D_2x2.png`
- [x] `assets/dither3d/Dither3D_2x2_Ramp.png`
- [x] `assets/dither3d/Dither3D_4x4.png`
- [x] `assets/dither3d/Dither3D_4x4_Ramp.png`
- [x] `assets/dither3d/Dither3D_8x8.png`
- [x] `assets/dither3d/Dither3D_8x8_Ramp.png`
- [x] `assets/dither3d/reference/Dither3DInclude.cginc`
- [x] `assets/dither3d/reference/Dither3DOpaque.shader`

### Documentation
- [x] `docs/rendering/dither3d.md` - Full integration guide
- [x] `docs/rendering/README.md` - Updated index
- [x] `docs/rendering/frame-graph.md` - Added dither pass
- [x] `docs/rendering/render-system.md` - Added dither pass
- [x] `docs/rendering/path-tracer.md` - Added dither shader

### Tools
- [x] `tools/copy_dither_textures.py` - Texture copy script
- [x] `tools/build_dither_shaders.py` - Shader compilation script

---

## Usage Example

```cpp
#include "litt_renderer.h"

litt::Renderer renderer;
renderer.initialize(1920, 1080, litt::RenderBackend::Vulkan);

// Enable dithering with 8x8 grayscale pattern
renderer.enable_dither(litt::DitherColorMode::Grayscale, litt::DitherPattern::P8x8);

// Or configure custom parameters
renderer.set_dither_params(
    5.0f,      // scale
    0.0f,      // size variability (Bayer-style)
    1.0f,      // contrast
    litt::DitherColorMode::RGB,  // color mode
    litt::DitherPattern::P8x8    // pattern
);

// Disable dithering
renderer.disable_dither();
```

## Shader Feature Flags

Set in `shaders/dither3d/mesh.frag.glsl` before compiling:

```glsl
#define DITHER_INVERSE    // Swap light/dark dots
#define DITHER_RADIAL     // Screen-edge stability
#define DITHER_QUANTIZE   // Disable fractal interpolation
#define DITHER_DEBUG      // Visualize pattern level
```

## Known Limitations

1. **Vulkan texture upload** - Stub implementation in `litt_dither_renderer.cpp`
2. **DX12 backend** - Not yet ported
3. **Path tracer** - Dither3D not integrated into ray tracing miss shader

## Next Steps

1. Implement `upload_3d_texture()` in `litt_dither_renderer.cpp`
2. Add DX12/HLSL shader variants
3. Integrate with path tracer miss shader for ray-traced scenes
4. Add Dither3D to the FFI header (`litt_ffi.h`) for language bindings
