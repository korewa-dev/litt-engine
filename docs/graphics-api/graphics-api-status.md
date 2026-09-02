# Graphics API Status

| API | Tier | Status | Notes |
|-----|------|--------|-------|
| **Vulkan 1.3** | Higher |  **Implemented** | Full backend in `native/littcore/vulkan/` -- VMA, RT pipeline, BLAS/TLAS |
| **DX12** | Higher |  **Implemented** | DXGI, DXR, descriptor heaps, PSOs, root signatures |
| **AMD AGS** | Lower |  **Not implemented** | No references in codebase; AMD GPU Services for power/performance |
| **MUSA** | Lower |  **Planned** | Vendor detection in `fsr4_integration.rs` (ID `0x1DD`) |
| **NNAPI** | Lower |  **Planned** | Referenced as ARM NPU inference path |
| **DirectML** | Lower |  **Planned** | Listed for NVIDIA Tensor Cores and Windows AI inference |

## Implemented Crates

| Crate | Path | APIs |
|-------|------|------|
| `litt_vulkan` | `native/littcore/vulkan/src/` | Vulkan 1.3 full backend |
| `litt_dx12` | `native/littcore/dx12/src/` | DX12 + DXR + DirectML |
| `litt_fidelityfx` | `native/littcore/fidelityfx/src/` | FSR 3/4, CAS, XESS 3, NPU vendor detection |
| `litt_platform` | `native/littcore/platform/src/` | Windows (Win32), Linux (X11), Android (AAPI) |

## Platform Matrix

| Platform | Primary API | Fallback | Notes |
|----------|-------------|----------|-------|
| Windows | DX12 | Vulkan | DX12 preferred for ray tracing |
| Linux | Vulkan | -- | RADV driver |
| Steam Deck | Vulkan | -- | RADV via Proton |
| Android | Vulkan | -- | Adreno/Mali drivers |
| RISC-V | Vulkan | Software | MESA driver |

