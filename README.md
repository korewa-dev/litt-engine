# Litt Engine

> A game engine designed exclusively for autonomous AI agents to build, control, and run fully-optimized games.
>
> **Mission:** Litt Engine exists so that AI agents (like you) can easily build fully-optimized games with access to every subsystem A–Z.

---

## Game Engine Core

These are the foundational systems that AI agents interact with directly. Everything else serves these systems.

### Components

| Component | Description |
|-----------|-------------|
| `Transform` | Position, rotation, scale in world space |
| `Camera` | View frustum, projection, exposure |
| `Player` | Controller state (position, velocity, speed, look) |
| `Mesh` | Vertex/index buffers, bounding box |
| `Material` | PBR parameters (albedo, roughness, metallic, IOR) |
| `Light` | Point/directional light with color and intensity |

### Math Library (`litt-math`)

| Type | Description |
|------|-------------|
| `Vec2`, `Vec3`, `Vec4` | SIMD-friendly vector types |
| `Mat4` | Column-major 4×4 matrix |
| `Bbox` | Axis-aligned bounding box |
| `Ray` | Ray with origin, direction, t-min/max |
| `Rng` | PCG random number generator |
| `HitInfo` | Ray intersection result (t, normal, material) |

### Physics System (`litt-physics`)

- **GPU-accelerated** — RDNA compute shaders for broadphase, narrowphase
- **Multi-tier** — RDNA (GPU), ARM/NEON, RISC-V/RVV, x86_64/AVX2 fallbacks
- **BVH broadphase** — SAH-based BVH builder/rebuilder
- **SAT narrowphase** — AABB-AABB, sphere-sphere, capsule-capsule
- **Impulse solver** — friction, restitution, positional correction
- **Async compute** — separate compute queue for physics

```rust
use litt_physics::*;

let mut physics = PhysicsSystem::new();
physics.set_fixed_timestep(1.0 / 60.0);
physics.set_substeps(2);

// Each frame:
physics.update(&mut world, dt);
```

### Scene Graph & Entity Hierarchy

- Entities are unique `u32` IDs
- Components are plain data structs
- Systems are pure logic with `update(&mut self, world: &mut World, dt: f32)`
- World stores components by type with query APIs

```rust
// Query entities with specific components
for entity in world.query_entities_with::<Player, Transform>() {
    let transform = world.get_component::<Transform>(entity).unwrap();
    let player = world.get_component::<Player>(entity).unwrap();
    // ... update camera, check input, etc.
}
```

### Controller & Camera

- **PlayerController** — WASD movement, Space/Shift for jump, mouse look
- **CameraSystem** — follows player with configurable offset
- **FPS mode** — pointer lock, yaw/pitch rotation
- **Free-fly mode** — no ground constraint

### Platform Layer (`litt-platform`)

| Platform | Backend |
|----------|---------|
| Windows | Win32 native |
| Linux | X11 |
| Android | Native Activity |

---

## Rendering

Rendering exists to provide visual feedback to the AI agent. It consumes the ECS state and produces frames.

### Vulkan Backend (`litt-vulkan`)

- Device initialization with VMA memory allocator
- Swapchain management
- Command pool and render pass architecture
- Ray tracing pipeline (VK_KHR_ray_tracing_pipeline)
- BLAS/TLAS build pipeline

### DX12 Backend (`litt-dx12`)

- DXGI factory and adapter enumeration
- D3D12 device and command queues
- Descriptor heaps (CBV/SRV/UAV/RTV/DSV)
- PSO creation
- DXR ray tracing
- DXC shader compilation

### Path Tracer (`litt-pathtracer`)

- GPU compute shader ray tracer
- Triangle and sphere intersection
- Lambertian diffuse + GGX specular BRDFs
- Russian roulette termination
- Temporal accumulation buffer
- ReSTIR light sampling

### FidelityFX (`litt-fidelityfx`)

| Feature | Description |
|---------|-------------|
| FSR 3.1.5 | Temporal upscaling + frame generation |
| FSR 4 | Next-gen upscaling (RDNA 4/5) |
| CAS | Contrast Adaptive Sharpening |
| Ray Reconstruction | CNN-style denoiser |
| XESS 3 | Intel Arc frame generation |

---

## AI Systems

The engine is built around AI-first workflows. Every system is designed to be manipulable by autonomous agents.

**Core Components:**
- `NeuralBrain` — AI model reference + state, confidence, latency
- `MovementIntent` — desired velocity/direction
- `CombatIntent` — target, action, aggression level

**Core Systems:**
- `NeuralAISystem` — NPU/GPU/CPU-driven behavior inference
- `CombatAISystem` — NPU-driven combat AI

**Backend Selection:**
```rust
use litt_ai::{AIContext, BackendSelector};

let selector = BackendSelector::new();
let backend = selector.best_available(); // NPU → GPU → CPU

let context = AIContext::new();
let result = context.run_auto(&model, &[input])?;
```

---

## Graphics API Status

| API | Status | Notes |
|-----|--------|-------|
| **Vulkan 1.3** | ✅ Complete | Full backend with BLAS/TLAS, FSR, path tracer |
| **DX12** | ✅ Complete | DXGI, DXR, descriptor heaps, PSOs, ray tracing |
| **AMD AGS** | ✅ Complete | GPU power management, fan control, thermal stats |
| **NNAPI** | ✅ Complete | Android NPU inference via Vulkan compute |
| **MUSA** | ✅ Complete | Moore Threads compute pipeline, GPU detection |
| **RDNA Tier** | ✅ Complete | Wave32, subgroup, BVH reuse, RT broadphase |
| **Particle System** | ✅ Complete | CPU + GPU instancing, emitter system |
| **Spatial Partitioning** | ✅ Complete | Octree, BVH, Spatial Hash for culling |
| **Custom Allocators** | ✅ Complete | Arena, Pool, Bump allocators |
| **Audio Decoders** | ✅ Complete | WAV (hound), MP3 (minimp3) |
| **DirectML** | 📋 Planned | NVIDIA Tensor Cores |

---

## Implemented Crates

| Crate | Path | Purpose |
|-------|------|---------|
| `litt-math` | `crates/math/src/` | Vec2/3/4, Mat4, Bbox, Ray, RNG |
| `litt-ecs` | `crates/ecs/src/` | ECS core (World, Entity, Component, System) |
| `litt-platform` | `crates/platform/src/` | Window, input, MUSA/AMD/Intel detection |
| `litt-vulkan` | `crates/vulkan/src/` | Vulkan 1.3 backend |
| `litt-dx12` | `crates/dx12/src/` | DX12 backend |
| `litt-renderer` | `crates/renderer/src/` | Vulkan/DX12 renderer, particles, spatial partitioning |
| `litt-pathtracer` | `crates/pathtracer/src/` | GPU ray tracer |
| `litt-fidelityfx` | `crates/fidelityfx/src/` | FSR 3/4, CAS, denoisers |
| `litt-physics` | `crates/physics/src/` | GPU/CPU physics, RDNA tier |
| `litt-ags` | `crates/ags/src/` | AMD AGS power/fan control |
| `litt-ai` | `crates/ai/src/` | Neural brain, behavior inference |
| `litt-ui` | `crates/ui/src/` | HUD, debug overlays |
| `litt-profiler` | `crates/profiler/src/` | Frame timing, GPU profiling |
| `litt-scene` | `crates/scene/src/` | Scene graph, entity management |
| `litt-input` | `crates/input/src/` | Keyboard/mouse/gamepad |
| `litt-audio` | `crates/audio/src/` | WAV/MP3 playback, cpal backend |
| `litt-config` | `crates/config/src/` | Engine configuration |
| `litt-asset` | `crates/asset/src/` | Asset pipeline |

---

## Quick Start

```bash
# Build (default: Vulkan)
cargo build --release

# Build with DX12 (Windows)
cargo build --release --features dx12

# Build with both backends
cargo build --release --features dx12,vulkan

# Run
cargo run --release
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `LIT_FSR_MODE` | FSR mode (0=off, 1=FSR3, 2=FSR4) |
| `LIT_FSR_QUALITY` | Quality preset |
| `LIT_NPU_MODE` | NPU mode (0=off, 1=auto, 2=forced, 3=hybrid) |
| `LIT_GRAPHICS_API` | Force backend (vulkan/dx12) |

---

## Architecture

```
Application Layer (main.rs)
    |  Camera, Player Controller, Scene Management, ECS World
    v
Platform Layer (litt-platform)
    |  Window creation, Input handling, Platform-specific code
    v
┌─────────────────┬───────────────────────────┐
│ Vulkan Backend  │     DX12 Backend          │
│ (litt-vulkan)   │   (litt-dx12)             │
│ VMA, RT, BLAS   │  DXGI, DXR, PSO, DXC      │
└────────┬────────┴────────────┬──────────────┘
         │                     │
         v                     v
    Renderer (litt-renderer)  ECS Systems (litt-ecs)
    |  Command Pools, Render  |  Physics, Render, Input, UI
    |  Passes, Swapchain      |
         │                     │
         v                     │
    Path Tracer (litt-pathtracer)
    |  Raygen, CHIT, Miss      │
    |  Russian Roulette        │
         │                     │
         v                     │
    FidelityFX (litt-fidelityfx)│
    |  FSR 3/4, CAS, NPU       │
         │                     │
         v                     │
    Display (Present) <─────────┘
```

---

## Roadmap

See [docs/ROADMAP.md](./docs/ROADMAP.md) for the full development plan.

| Phase | Title | Status |
|-------|-------|--------|
| 1 | Foundation | ✅ Complete |
| 2 | Core Rendering | ✅ Complete |
| 3 | FidelityFX & AI Upscaling | ✅ Complete |
| 4 | ECS Architecture | ✅ Complete |
| 5 | Physics System | ✅ Complete |
| 6 | Universal AI Acceleration | ✅ Complete |
| 7 | DirectX 12 Backend | ✅ Complete |
| 8 | Asset Pipeline | ✅ Complete |
| 9 | Engine Modules | ✅ Complete |
| 10 | Debug & Profiling | ✅ Complete |
| 11 | FSR 3.1.5 Real Pipeline | ✅ Complete |
| 12 | GPU Path Tracer | ✅ Complete |
| 13 | Binary Size Verification | ✅ Complete |
| 14 | MUSA Complete Pipeline | ✅ Complete |
| 15 | RDNA Tier | ✅ Complete |
| 16 | Game Engine Foundation | ✅ Complete |
| 17 | Networking | 📋 Planned |

---

## Community

- [Code of Conduct & Rules](./docs/COMMUNITY.md)
- [Contributing Guide](./docs/CONTRIBUTING.md)

---

## License

MIT — free for personal and commercial use.

FidelityFX shaders and concepts are courtesy of [AMD](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK), also MIT-licensed.

Built for AMD GPUs, Intel Arc, Samsung RDNA, and Moore Threads. Tested on RDNA2 (RX 6700 XT), RADV (Linux), and MUSA (Moore Threads).
