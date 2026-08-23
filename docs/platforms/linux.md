# Linux Platform Notes

> Wayland/X11 support, RADV driver, and Vulkan configuration.

## Window Backends

| Backend | Status | Notes |
|---------|--------|-------|
| X11 |  Implemented | `litt-platform` X11 module |
| Wayland |  Planned | Required for modern desktops |
| XCB |  Implemented | Alternative X11 backend |

## RADV Driver

AMD''s open-source Vulkan driver for RDNA GPUs:

```bash
export RADV_PERFTEST=rt
export RADV_DEBUG=denormal_flush_to_zero
export RADV_FORCE_WAVE_SIZE=32
export RADV_CACHE_DIR=~/.cache/radv
```

## Steam Deck (Linux)

The Steam Deck runs a custom Arch Linux with RADV:

```bash
vulkaninfo | grep -i ray
export RADV_PERFTEST=rt
```

## Roadmap

### Short-term
- [ ] Wayland backend implementation
- [ ] RADV performance benchmarking

### Hardware-Specific
- **AMD (RADV):** Best Vulkan RT performance, wave32 optimization
- **Intel Arc:** Vulkan 1.3 native on Battlemage
- **NVIDIA:** Proprietary driver, good DX12 via VKD3D-Proton

