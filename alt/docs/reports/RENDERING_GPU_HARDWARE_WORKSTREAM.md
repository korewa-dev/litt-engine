# Rendering / GPU / Hardware Workstream Report

Date: 2026-09-21

## Scope completed

This pass converts the rendering/GPU surface from ambiguous capability claims into explicit runtime contracts.

- Added `gpu_backend_capability()` with test-only, partial, and unavailable states.
- Added the public `create_gpu_device()` declaration.
- Unknown backends now fail with `invalid_argument`; known unavailable hardware backends fail with a reason.
- Added an explicit `null` test device path.
- Fixed null buffers so descriptor size and usage are preserved instead of reporting fabricated vertex metadata.
- Added real bounded CPU buffers to the software renderer.
- Software textures now reject unsupported formats instead of allocating every format as if it were RGBA8.
- Added a headless rasterization/resource contract with a 1000-frame smoke loop.
- Added Linux sanitizer coverage and Windows compilation/runtime coverage for the GPU contract.
- Replaced false Vulkan/DX12 documentation with an evidence-based status ledger.

## Current traffic lights

| Capability | Status | Evidence |
|---|---|---|
| Headless software framebuffer/rasterization | Orange | real implementation plus 1000-frame CI contract |
| Software CPU buffers | Orange | create/map/update/type/size contract |
| Software RGBA8 textures | Orange | creation contract; broader format/render-target support absent |
| Win32 software presentation | Orange | implementation exists; no physical display certification |
| Vulkan | Red | initialization intentionally unavailable |
| DirectX 12 | Red | initialization intentionally unavailable |
| OpenGL | Red | no backend |
| Metal | Red | no backend |
| GPU ray tracing | Red | no supported backend |
| AMD vendor certification | Red | no physical evidence |
| Intel Arc certification | Red | no physical evidence |
| NVIDIA certification | Red | no physical evidence |
| Moore Threads certification | Red | no physical evidence |

## Why hardware backends are not green

A green hardware claim requires real backend behavior plus physical evidence. CI runners can validate compilation, sanitizers, software rendering, and unavailable-device contracts, but they cannot certify vendor hardware that was not physically exercised.

The required promotion sequence is documented in `graphics-api-status.md`. No vendor/API should be promoted merely because a source file, enum, extension name, or driver ecosystem exists.

## Next evidence required

Future backend implementation work should start with one API and one platform, complete the promotion sequence end to end, then add physical test records. Vulkan on Linux is the smallest cross-vendor candidate; DX12 on Windows should remain a separate backend effort.
