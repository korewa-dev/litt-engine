# Graphics API Status

This page is the authoritative graphics capability ledger. Backend enums, placeholder source files, vendor IDs, build flags, or design documentation are not runtime support evidence.

## Backend contract status

"GREEN" below means the behavior claimed by the release contract is tested. It does not mean hardware rendering is implemented.

| Backend | CI contract | Release rendering | Evidence |
|---|---|---|---|
| Software, headless | **GREEN** | **Supported bounded contract** | CPU buffers, RGBA8 textures, raster/depth/mesh paths, resource ceilings, 1000-frame smoke |
| Software, Win32 presentation | **GREEN bounded contract** | Partial | Windows CI builds/runs the GPU contract; physical display output is not certified |
| Vulkan | **GREEN fail-fast contract** | **Unavailable** | capability reports unavailable; selection must reject or initialization must fail explicitly |
| DirectX 12 | **GREEN fail-fast contract** | **Unavailable** | capability reports unavailable; selection must reject or initialization must fail explicitly |
| OpenGL | **GREEN fail-fast contract** | **Unavailable / non-release** | capability reports unavailable; device creation does not fabricate a backend |
| Metal | **GREEN fail-fast contract** | **Unavailable / non-release** | capability reports unavailable; device creation does not fabricate a backend |

The Vulkan, DX12, OpenGL and Metal fail-fast contracts run in the GPU contract on GCC, Clang, ASan/UBSan, and Windows. A build flag or placeholder device must never turn an unavailable backend into fabricated success.

## Vendor and GPU status

The generic Vulkan/DX12 contract is vendor-independent. Litt does not currently special-case a vendor into supported hardware rendering. Therefore every named GPU family below has the same truthful contract: unsupported hardware rendering must fail explicitly.

| GPU family or vendor | Vulkan contract | DX12 contract | Physical Litt certification |
|---|---|---|---|
| AMD RDNA / Radeon | **GREEN fail-fast** | **GREEN fail-fast** | Not certified |
| Intel Arc | **GREEN fail-fast** | **GREEN fail-fast** | Not certified |
| NVIDIA GeForce / RTX | **GREEN fail-fast** | **GREEN fail-fast** | Not certified |
| Moore Threads | **GREEN fail-fast** | **GREEN fail-fast** | Not certified |
| AMD Steam Deck APU | **GREEN fail-fast** | **GREEN fail-fast** | Not certified |
| ARM Mali | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |
| Qualcomm Adreno | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |
| Samsung / Xclipse | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |
| MediaTek GPU platforms | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |
| Huawei / mobile GPU platforms | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |
| Apple GPU | No release Vulkan path | N/A | Not certified |
| RISC-V GPU platforms | **GREEN fail-fast** | N/A as a native DX12 target | Not certified |

This matrix is intentionally conservative. Driver or API compatibility in principle is not the same as Litt Engine support.

## Physical promotion gate

A hardware backend can only become release-supported after this sequence is real and tested:

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
14. physical hardware evidence for each promoted vendor/configuration

Software CI can establish compilation and non-hardware failure contracts. It cannot substitute for physical GPU evidence.

## Contract API

Use `gpu_backend_capability()` to inspect generic backend status and `create_gpu_device()` to request a device. Unavailable and unknown backends fail explicitly. The `null` device is test-only and does not pretend to render.
