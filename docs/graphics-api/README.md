# Graphics API Documentation

Subsystems covering rendering backends, ray tracing, and upscaling.

## Files

| File | Content |
|------|---------|
| [vulkan.md](./vulkan.md) | Vulkan backend internals (from ARCHITECTURE.md + current crate details) |
| [dx12.md](./dx12.md) | DirectX 12 backend (from DX12_SUPPORT.md) |
| [ray-tracing.md](./ray-tracing.md) | Path tracer deep-dive: BLAS/TLAS, shaders, BRDFs |
| [fidelityfx.md](./fidelityfx.md) | FSR 3/4, CAS, denoisers, XESS 3 integration |
| [amd-agp.md](./amd-agp.md) | AMD AGS -- planned power/performance control |
| [graphics-api-status.md](./graphics-api-status.md) | API status table (from root README) |

## Architecture

```
Application
    |
    v
Platform Layer (litt-platform)
    |
    v
Vulkan Backend (litt-vulkan)  |  DX12 Backend (litt-dx12)
    |                           |
    v                           v
Renderer (litt-renderer)  <---> ECS Systems (litt-ecs)
    |
    v
Path Tracer (litt-pathtracer)
    |
    v
FidelityFX (litt-fidelityfx)
    |
    v
Display (Present)
```

See [../ARCHITECTURE.md](../ARCHITECTURE.md) for the full crate dependency graph.
