# Rendering Documentation

The rendering subsystem includes the path tracer, frame graph, and FidelityFX integration.

## Files

| File | Content |
|------|---------|
| [render-system.md](./render-system.md) | ECS rendering pipeline, pass ordering, shader compilation |
| [path-tracer.md](./path-tracer.md) | pathtracer crate internals, BRDFs, ray tracing |
| [frame-graph.md](./frame-graph.md) | Complete frame graph and pass dependencies |

## Architecture

```
shader compilation (build.rs)
    |
    v
Vulkan/DX12 back-end
    |
    +-> Path Tracer (ray tracing)
    +-> FidelityFX (upscaling, denoisers)
    +-> Post-Processing (tonemap, TAA)
    |
    v
GPU command buffers -> Present
```

## Hardware Targeting

| Hardware | Primary API | Optimizations |
|----------|-------------|---------------|
| AMD RDNA  | Vulkan     | Wave32 compute, RGP profiling |
| Intel Arc | DX12 + Vulkan | DirectML, XESS 3 |
| Moore Threads | Vulkan | MUSA compute |
| ARM Mali | Vulkan | Bifrost + NEON optimizations |
| RISC-V | Vulkan | RVV vectorization |

See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md) for full status.
