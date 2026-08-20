# Platform Documentation

Per-platform GPU and OS notes for Litt Engine.

## Files

| File | Content |
|------|---------|
| [amd-rdna.md](./amd-rdna.md) | AMD RDNA shader/compiler optimizations, RGP |
| [intel-arc.md](./intel-arc.md) | Intel Arc XeSS 3 integration |
| [moore-threads.md](./moore-threads.md) | MUSA GPU support, Vulkan extensions |
| [risc-v.md](./risc-v.md) | RISC-V Linux, RVV, Vortex GPU |
| [windows.md](./windows.md) | Windows-specific notes, DX12, registry |
| [linux.md](./linux.md) | Linux Wayland/X11, RADV, driver flags |
| [android.md](./android.md) | Android Adreno/Mali/PowerVR, NPU |
| [steam-deck.md](./steam-deck.md) | Steam Deck profiles, controller, Proton |

## Platform Matrix

| Platform | Window Backend | Primary GPU API | NPU | Status |
|----------|---------------|-----------------|-----|--------|
| Windows | Win32 | DX12 (preferred) / Vulkan | Intel AI Boost, AMD X DNA | Implemented |
| Linux | X11 / Wayland | Vulkan (RADV) | AMD XDNA | Partial |
| Steam Deck | Win32 (Proton) | Vulkan (RADV) | AMD XDNA 2 | Partial |
| Android | ANativeWindow | Vulkan | Hexagon, Mali-NPU | Partial |
| RISC-V | Wayland | Vulkan (MESA) | RVV AI | Planned |
