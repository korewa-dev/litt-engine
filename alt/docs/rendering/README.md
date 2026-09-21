# Rendering Documentation

The rendering documentation contains implementation notes, experiments, and target architecture. Release support is defined by [../SUPPORTED_RUNTIME.md](../SUPPORTED_RUNTIME.md) and the graphics ledger in [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md).

## Files

| File | Content |
|---|---|
| [render-system.md](./render-system.md) | ECS rendering design and pass ordering |
| [path-tracer.md](./path-tracer.md) | path-tracing design and experiments |
| [frame-graph.md](./frame-graph.md) | frame-graph design and pass dependencies |
| [dither3d.md](./dither3d.md) | Surface-Stable Fractal Dithering integration |

## Current release rendering contract

The tested rendering path is software/headless. Generic Vulkan and DX12 hardware rendering are currently unavailable and have a **GREEN fail-fast contract**: they must report unavailability rather than fabricate a working device.

The target architecture may eventually connect:

```text
shader compilation
    |
    v
Vulkan / DX12 backend [experimental, unavailable today]
    |
    +-> path tracing
    +-> upscaling / denoising
    +-> post-processing
    +-> Dither3D
    |
    v
GPU command buffers -> present
```

## Hardware research targets

These are design targets, not support claims.

| Hardware | Candidate API or research direction | Litt physical certification |
|---|---|---|
| AMD RDNA / Radeon | Vulkan and/or DX12 | Not certified |
| Intel Arc | Vulkan and/or DX12 | Not certified |
| NVIDIA | Vulkan and/or DX12 | Not certified |
| Moore Threads | Vulkan | Not certified |
| ARM Mali | Vulkan | Not certified |
| Qualcomm Adreno | Vulkan | Not certified |
| Steam Deck APU | Vulkan | Not certified |
| Apple GPU | future platform-specific decision | Not certified |
| RISC-V GPU platforms | Vulkan where available | Not certified |

See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md) for the exact tested contract.
