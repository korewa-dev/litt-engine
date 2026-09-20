# Graphics API Status

This page is a capability ledger, not a roadmap. Backend enums, placeholder source files, vendor IDs, or documentation are not runtime evidence.

| Backend | Status | Runtime evidence |
|---|---|---|
| Software, headless | **Partial, tested** | CPU framebuffer, rasterization, RGBA8 texture, CPU buffer, 1000-frame smoke |
| Software, Win32 presentation | **Partial** | implementation exists; CI exercises construction/headless contract, not physical display output |
| Vulkan | **Unavailable** | generic device initialization intentionally fails |
| DirectX 12 | **Unavailable** | generic device initialization intentionally fails |
| OpenGL | **Unavailable** | no backend implementation |
| Metal | **Unavailable** | no backend implementation |

## Hardware claims

No AMD, Intel Arc, NVIDIA, Moore Threads, Apple, Android, Steam Deck, or RISC-V GPU is currently certified by Litt Engine. Driver/API compatibility in principle is not the same as engine support.

A hardware backend can only be promoted after this sequence is real and tested:

1. backend initialization
2. adapter enumeration and feature query
3. logical device, queues, and allocator
4. swapchain and render targets
5. shader and pipeline creation
6. buffers and textures
7. triangle rendering
8. OBJ/material rendering
9. resize handling
10. 1000+ sustained frames
11. destruction and clean shutdown
12. unavailable-device and device-loss/error paths
13. validation-layer/leak checks
14. physical hardware evidence for each vendor claim

CI can establish compilation and non-hardware failure contracts. It cannot substitute for physical GPU evidence.

## Contract

Use `gpu_backend_capability()` to inspect the generic backend status and `create_gpu_device()` to request a device. Unavailable and unknown backends fail explicitly. The `null` device is test-only and does not pretend to render.
