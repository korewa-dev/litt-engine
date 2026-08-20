# AMD GPU Optimization Notes

## Target Architectures
- RDNA 2 (RX 6000 series): Wave32/Wave64, 64 CU
- RDNA 3 (RX 7000 series): Enhanced wave32, up to 128 CU
- RADV (Linux): Open source Vulkan driver for AMD

## Compiler Optimizations
```toml
[profile.release]
opt-level = "z"      # Size optimization
lto = true           # Link-time optimization
codegen-units = 1    # Single CG for better optimization
panic = "abort"      # No unwind tables
strip = true         # Remove debug symbols
```

## Shader Optimizations for AMD

### Wave32 vs Wave64
- RDNA2/3: Use wave32 for compute, wave64 for ray tracing
- Minimize register pressure for better occupancy

### Memory Access Patterns
- Coalesced memory access preferred
- Use 128-bit aligned accesses
- Prefer contiguous workgroup memory access

## RADV Driver Notes

### Environment Variables
```bash
export RADV_PERFTEST=rt
export RADV_DEBUG=denormal_flush_to_zero
export RADV_FORCE_WAVE_SIZE=32
```

## Radeon GPU Profiler (RGP) Integration
```bash
rgp.exe --target=litt.exe --capture=1
```

## FidelityFX Integration
- FSR 2: Temporal upscaling with hardware RT acceleration
- CAS: Lightweight sharpening pass
- Ray Reconstruction: CNN-based denoiser
