# Graphics API Documentation

This folder contains graphics backend status, implementation notes, and research material.

> [graphics-api-status.md](./graphics-api-status.md) is authoritative for current graphics support. Vulkan and DX12 currently have a **GREEN fail-fast contract** but no release-supported hardware renderer.

## Files

| File | Content |
|---|---|
| [graphics-api-status.md](./graphics-api-status.md) | authoritative tested backend and hardware status |
| [vulkan.md](./vulkan.md) | Vulkan placeholder/experimental backend notes |
| [dx12.md](./dx12.md) | DirectX 12 placeholder/experimental backend notes |
| [ray-tracing.md](./ray-tracing.md) | path-tracing and ray-tracing research |
| [fidelityfx.md](./fidelityfx.md) | FidelityFX/FSR research, not certified support |
| [amd-agp.md](./amd-agp.md) | planned AMD AGS research |

## Target architecture

The architecture below is a design target, not evidence that the hardware path is implemented:

```text
Application
    |
    v
Platform layer
    |
    v
Vulkan backend [experimental] | DX12 backend [experimental]
    |                           |
    v                           v
Renderer <-> ECS systems
    |
    v
optional path tracing / upscaling research
    |
    v
Present
```

Current release rendering uses the tested software/headless path. Hardware promotion requires the sequence in [graphics-api-status.md](./graphics-api-status.md).
