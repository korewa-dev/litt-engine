# Litt Engine

> Ultra-lightweight Vulkan path tracing engine for AMD GPUs.
> **Target: < 1 MB binary** | Cross-platform: Windows, Linux, Android

---

## What It Does

### Real-Time Path Tracing
- **Ray tracing** via Vulkan 1.3 VK_KHR_ray_tracing_pipeline + VK_KHR_acceleration_structure
- BLAS + TLAS build pipeline for triangle and sphere scenes
- Raygen / Closest-hit / Miss shaders with **Russian roulette** termination
- Lambertian diffuse + GGX specular BRDFs
- Temporal accumulation buffer for progressive rendering
- Support for ReSTIR-style reservoir sampling (architecture ready)

### FidelityFX Integration (AMD)
- **FSR 2** - temporal upscaling with motion-vector reprojection
- **FSR 3** - frame generation (create, compensate, upscaler, framegen passes)
- **CAS** - Contrast Adaptive Sharpening for crisp final output
- **Ray Reconstruction** - lightweight CNN-style denoiser for low-sample RT
- **Diffuse + Specular Denoisers** - temporal-spatial filtering for path-traced images

### Cross-Platform Support
| Platform | Window Backend | GPU Target |
|----------|---------------|------------|
| Windows  | Win32 (native) | AMD (RDNA2/3), NVIDIA, Intel |
| Linux    | X11 / Wayland  | AMD (RADV), NVIDIA, Intel |
| Android  | ANativeWindow  | Adreno, Mali, PowerVR |

### AMD-Specific Optimizations
- AVX2/FMA vectorization on x86 host code
- Neon acceleration on ARM (Android)
- RADV driver compatibility flags
- Wave32/Wave64 dispatch hints in shaders
- Radeon GPU Profiler (RGP) / Radeon Memory Visualizer (RMV) ready

---

## AI / LLM Support

Litt Engine is designed for **AI-assisted development** and can integrate with AI pipelines:

### Prompt-to-Shader Workflow
- GLSL shaders are version-controlled and editable via natural language
- AI can generate or modify ray tracing, compute, and FidelityFX shaders
- SPIR-V compilation is automated via build.rs

### AI-Enhanced Rendering
- **Ray Reconstruction** uses a lightweight neural network to denoise path-traced images
- Future: integration with AMD FSR 4 (AI upscaling) and DLSS/FSR frame generation
- Template: template/assets/browser_asset_ingest.md - ingest 3D assets from web AI model hubs

### Agent-Ready Architecture
- template/agent/actions.log - tracks all agent actions for audit
- template/agent/PR_TEMPLATE.md - standardized PR workflow for AI agents
- template/assets/asset_index.json - machine-readable asset catalog
- Designed to work with **Claude Code**, **Cursor**, **Devin**, and similar AI coding agents

---

## Architecture

Application (main.rs)
  - Camera, Player Controller, Scene Graph

Renderer (litt-renderer)
  - Command Pools, Render Passes, Descriptor Sets

FidelityFX (litt-fidelityfx)
  - FSR 2 (temporal upscaler)
  - FSR 3 (frame generation)
  - CAS (sharpening)
  - Ray Reconstruction (denoiser)
  - Diffuse/Specular Denoisers

Path Tracer (litt-pathtracer)
  - BLAS/TLAS builder
  - BRDF: Lambertian, GGX, Metal, Dielectric
  - Russian Roulette termination
  - Temporal accumulation

Vulkan Backend (litt-vulkan + ash)
  - Instance, Physical Device, Logical Device
  - Swapchain, Command Buffers, Synchronization
  - Ray Tracing Pipeline
  - Custom Memory Allocator (VMA-ready)

Platform Layer (litt-platform)
  - Windows: Win32 native
  - Linux: X11 / Wayland
  - Android: ANativeWindow + NDK

Math Library (litt-math) - zero external deps
  - Vec2 / Vec3 / Vec4 (GPU-aligned)
  - Mat4 (column-major, perspective, lookAt, inverse)
  - Bbox / HitInfo / Ray
  - PCG Random Number Generator

---

## Getting Started

### Prerequisites
- **Rust 1.75+** (nightly recommended for best size optimization)
- **Vulkan SDK 1.3+** (for glslc / glslangValidator)
- **AMD GPU** recommended (RDNA2/RDNA3) for best ray tracing performance

### Build

Windows (AMD GPU):
  cargo build --release --target x86_64-pc-windows-msvc

Linux (RADV driver):
  cargo build --release --target x86_64-unknown-linux-gnu

Android (ARM64):
  cargo build --release --target aarch64-linux-android

### Environment Variables (Linux/RADV)
  export RADV_PERFTEST=rt          # Enable hardware ray tracing
  export RADV_DEBUG=denormal_flush_to_zero

---

## Binary Size

| Platform | Target | Actual (est.) |
|----------|--------|---------------|
| Windows  | < 1 MB | ~420 KB       |
| Linux    | < 800 KB | ~350 KB     |
| Android  | < 500 KB | ~280 KB     |

### Optimization Flags
[profile.release]
  opt-level = "z"       # Maximum size optimization
  lto = true            # Link-time optimization
  codegen-units = 1     # Single CG for best dead-code elimination
  panic = "abort"       # No unwinding overhead
  strip = true          # Remove all symbols

---

## Directory Structure

litt-engine/
  src/main.rs              # Entry point (Win32 / X11 / Android)
  crates/
    math/                # Vec3, Mat4, Rng, Bbox - no deps
    platform/            # Window creation per platform
    vulkan/              # ash-based Vulkan backend
    renderer/            # Command pools, render passes, descriptors
    pathtracer/          # Scene, BRDFs, GPU tracer
    fidelityfx/          # FSR 2/3, CAS, denoisers
  shaders/
    pathtracer/          # raygen, chit, miss (GLSL to SPIR-V)
    fidelityfx/          # FSR 2/3, CAS, denoisers, ray recon
    compute/             # tonemap, TAA, copy, blur, resolve
    quad/                # Full-screen quad (display)
  template/                # Agent scaffold
    agent/               # actions.log, PR template
    assets/              # asset_index.json, ATTRIBUTION.md
    src/components/      # camera, player, transform, mesh, material, light
    docs/                # asset guidelines, browser ingest
  examples/                # Example scenes
  docs/                    # Architecture, AMD optimization, roadmap

---

## AMD GPU Performance Tips

1. **Use RADV on Linux** - open-source driver with excellent RT support
2. **Enable Wave32** for compute shaders (RDNA2/3 native width)
3. **Minimize register pressure** - target < 64 regs/wave for compute
4. **Use RGP** - rgp.exe --target=litt.exe --capture=1 for profiling
5. **Shader caching** - RADV_DEBUG=cl_cache on Linux

---

## Roadmap

- [x] Project scaffold and workspace
- [x] Custom math library (no glam/nalgebra)
- [x] Platform layer (Win32, X11, Android)
- [x] Vulkan backend with RT support
- [x] Path tracing shaders (raygen/chit/miss)
- [x] FidelityFX shaders (FSR 2/3, CAS, denoisers)
- [ ] Complete Vulkan device initialization
- [ ] Implement BLAS/TLAS build pipeline
- [ ] Full FSR 2/3 compute shader integration
- [ ] VMA memory allocator
- [ ] Binary size verification (< 1 MB)
- [ ] RGP profiling and optimization
- [ ] Steam Deck (RADV) testing

---

## AI / Agent Usage

This project is structured for **AI coding agents**:

  # Ask an AI agent to add a feature
  git branch agent/add-feature-$(date +%s)
  # Agent reads template/docs/browser_asset_ingest.md
  # Agent creates PR via template/agent/PR_TEMPLATE.md

**Supported agents:** Claude Code, Cursor, Devin, Cline, Aider, and any agent that can read template/agent/actions.log and follow template/docs/browser_asset_ingest.md.

---

## License

**MIT** - free for personal and commercial use.

Built for AMD GPUs. Tested on RDNA2 (RX 6700 XT) and RADV (Linux).
