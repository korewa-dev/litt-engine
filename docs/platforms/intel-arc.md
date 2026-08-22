# Intel GPU & XeSS 3 Support

## Supported Intel GPUs
- **Intel Arc A770 / A750** (Alchemist, Xe-HPG)
- **Intel Arc B570 / B580 / B770** (Battlemage, Xe2)
- **Intel Arc MAX** (Datacenter)

## Vulkan Support
- Vulkan 1.3 native on Battlemage
- Vulkan 1.2 on Alchemist (driver updates may add 1.3)
- Full ray tracing support via VK_KHR_ray_tracing_pipeline

## XeSS 3 Integration
XeSS 3 is Intel''s answer to AMD FSR 3 -- combining AI upscaling with frame generation.

### Features
- **Spatial Upscaling** -- XeSS Quality/Balanced/Performance modes
- **Frame Generation** -- AI-powered intermediate frames
- **Reconstruction** -- temporal feedback for stability

### Shader Integration
- Compute shader: `shaders/fidelityfx/xess3_framegen.comp.glsl`
- Config: `litt_fidelityfx::Xess3` struct
- Quality levels: 0=Performance, 1=Balanced, 2=Quality, 3=Ultra Quality

### Environment Variables (Linux)
```bash
export INTEL_FEATURES=rt          # Enable ray tracing
export INTEL_SHADER_CACHE=1       # Enable shader caching
export Zink_debug=1               # Debug mode (Mesa/Zink for VMware)
```

## Performance Tips
1. **Use XeSS Quality mode** for best image quality
2. **Enable frame generation** for 2x+ FPS on supported scenes
3. **Shader cache** is critical -- first frame is slow without it
4. **Ray tracing**: Enable `VK_EXT_robustness2` for safe buffer access
5. **Intel Arc B580+**: Supports Wave32 natively -- match shader wave size

## Driver Notes
- **Windows**: Use latest Intel Arc driver (2.x branch)
- **Linux**: Mesa 24.0+ recommended (RADV for AMD, but Intel uses Open Source)
- **Vulkan driver**: `intel_icd.x86_64.json` or `intel_mgmt.json`

