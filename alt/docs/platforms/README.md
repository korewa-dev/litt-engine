# Platform Documentation

> **Evidence boundary:** These files are platform research, configuration notes, and optimization targets. Listing a device, API, driver, or extension does not mean Litt Engine has physically certified it. `../SUPPORTED_RUNTIME.md` is authoritative for release support.

Platform-specific research and optimization notes.

## Files

| File | Content |
|------|---------|
| [amd-rdna.md](./amd-rdna.md) | AMD RDNA optimizations |
| [intel-arc.md](./intel-arc.md) | Intel Arc and XeSS |
| [moore-threads.md](./moore-threads.md) | MUSA compute and RADV |
| [risc-v.md](./risc-v.md) | RISC-V hardware support |
| [windows.md](./windows.md) | Windows-specific notes |
| [linux.md](./linux.md) | Linux platform notes |
| [android.md](./android.md) | Android GPU support |
| [steam-deck.md](./steam-deck.md) | Steam Deck configuration |

## Hardware Diversity

The engine embraces hardware differences rather than abstracting them away:

- **CPU:** AVX2, AVX-512, NEON, RVV vectorization
- **GPU:** RDNA wave32, ARM Bifrost, NVIDIA CUDA, Intel Xe, Moore Threads MUSA
- **NPU:** AMD XDNA, Intel AI Boost, Qualcomm Hexagon, Huawei DaVinci, Samsung Exynos
- **OS:** Windows, Linux (Wayland/X11), Android, RISC-V Linux

