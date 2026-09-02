# Graphics API Documentation

Subsystems covering rendering backends, ray tracing, and upscaling.

## Files

| File | Content |
|------|---------|
| [vulkan.md](./vulkan.md) | Vulkan backend internals (from ARCHITECTURE.md + current module details) |
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
Platform Layer (litt_platform)
    |
    v
Vulkan Backend (litt_vulkan)  |  DX12 Backend (litt_dx12)
    |                           |
    v                           v
Renderer (litt_renderer)  <---> ECS Systems (litt_ecs)
    |
    v
Path Tracer (litt_pathtracer)
    |
    v
FidelityFX (litt_fidelityfx)
    |
    v
Display (Present)
```

See [../ARCHITECTURE.md](../ARCHITECTURE.md) for the full module dependency graph.
