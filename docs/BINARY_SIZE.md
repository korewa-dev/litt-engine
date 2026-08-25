<!-- REMOVED STACK NOTICE (CDR-007): The Rust engine described here was removed from the repo; this document remains as design reference for the C/C++ port (native/littcore). -->
# Binary Size Optimization Guide

## Target: Under 1 MB

## Current Dependency Size Estimate
- litt-math: ~5 KB (custom types, no dependencies)
- litt-platform: ~20 KB (platform-specific code)
- litt-vulkan: ~30 KB (Vulkan backend)
- litt-renderer: ~25 KB (renderer orchestration)
- litt-pathtracer: ~30 KB (path tracing logic)
- litt-fidelityfx: ~20 KB (FidelityFX integration)
- ash: ~150 KB (Vulkan bindings)
- bytemuck: ~5 KB (zero-cost casting)
- bitflags: ~3 KB (bitmask types)
- log: ~2 KB (logging)
- windows-sys: ~100 KB (Win32 bindings, Windows only)
- nix: ~30 KB (Unix syscalls, Linux only)

**Estimated total: ~420 KB** (well under 1 MB target)

## Verification
```bash
native\build.bat          # Windows (POSIX: make -C native)
ls -la native/bin
```

## Size Targets by Platform
| Platform | Target | Notes |
|----------|--------|-------|
| Windows | < 1 MB | Includes Win32 runtime |
| Linux | < 800 KB | Shared vulkan-1 |
| Android | < 500 KB | ARM64, stripped |

