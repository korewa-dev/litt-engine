?


## What It Does

### NPU Acceleration

| NPU | Vendor | TOPS (INT8) | Use Case |
|-----|--------|-------------|----------|
| Ryzen AI XDNA 2 | AMD | 50 | Denoising, frame gen |
| Ryzen AI (1st gen) | AMD | 25 | AI upscaling |
| Intel AI Boost | Intel | 48 | Neural reconstruction |
| Apple Neural Engine | Apple | 15.8 | Mobile AI |
| Exynos NPU | Samsung | 12 | RDNA iGPU NPU |
| Hexagon | Qualcomm | 15 | Mobile frame gen |
| APU | MediaTek | 10 | Mobile upscaling |
| Da Vinci NPU | Huawei Kirin | 8 | Mobile denoising |
| Mali-NPU | ARM | 6 | Mobile inference |
| Sophgo CV1800 | RISC-V |  | Edge AI inference |

- Auto-detection via `BackendSelector::best_available()`
- Fallback to GPU/NPU/CPU when unavailable
- Modes: Disabled / Auto / Forced / Hybrid

---

### Real-Time Path Tracing

- **Ray tracing** via Vulkan 1.3 `VK_KHR_ray_tracing_pipeline` + `VK_KHR_acceleration_structure`
- BLAS + TLAS build pipeline for triangle and sphere scenes
- Raygen / Closest-hit / Miss shaders with **Russian roulette** termination
- Lambertian diffuse + GGX specular BRDFs
- Temporal accumulation buffer for progressive rendering
- Support for ReSTIR-style reservoir sampling (architecture ready)

---

### FidelityFX Integration

- **FSR 3.1.5**  frame generation (create, compensate, upscaler, framegen passes)
- **FSR 4**  next-gen upscaling + frame generation (RDNA 4/5)
- **CAS**  Contrast Adaptive Sharpening for crisp final output
- **Ray Reconstruction**  lightweight CNN-style denoiser for low-sample RT
- **Diffuse + Specular Denoisers**  temporal-spatial filtering
- **Intel XESS 3**  frame generation for Intel Arc GPUs

---

### ECS Architecture (Complete)

ECS (EntityComponentSystem) is implemented in `crates/ecs`.
- **Entity** = unique u32 ID
- **Component** = plain data structs (blanket impl for Send + Sync + static)static)
- **System** = pure logic trait with `update(&mut self, world: &mut World, dt: f32)`
- **World** = HashMap-based component storage with query API
- **SystemGroup** = grouped system execution with ordered scheduling

**Core Components:**
| Component | Description |
|-----------|-------------|
| `Transform` | position, rotation, scale |
| `NeuralBrain` | AI model reference + state |
| `BehaviorState` | current behavior tree state |
| `MovementIntent` | desired velocity/direction |
| `CombatIntent` | target + action queue |
| `Renderable` | mesh handle + material ref |
| `PhysicsBody` | collider shape + mass + velocity |
| `InputState` | aggregated input per entity |
| `Light` | point/spot/directional light data |

**Core Systems:**
- `NeuralAISystem`  NPU-driven behavior inference
- `PhysicsSystem`  GPU-accelerated rigid body simulation
- `RenderSystem`  ECS ? Vulkan/DX12 draw commands
- `InputSystem`  keyboard/mouse/gamepad aggregation
- `UIOverlaySystem`  HUD, menus, debug overlays
- `NetworkingSystem`  optional ECS entity replication

See [docs/ECS_ARCHITECTURE.md](./docs/ECS_ARCHITECTURE.md) for the full API reference.

---

### Physics System (Planned)

Multi-tier GPU-accelerated physics per platform:

| Tier | GPU | Optimization |
|------|-----|-------------|
| RDNA | AMD RX 6000/7000/8000 | GPU broadphase, SIMD narrowphase, wave32, async compute |
| ARM | Adreno, Mali | NEON physics, fixed-step simulation |
| Samsung RDNA | Exynos 2200+ | RDNA compute physics |
| Kirin | Mali + Da Vinci NPU | Mali compute + NEON fallback |
| Moore Threads | MUSA | MUSA compute physics |
| RISC-V | Vortex GPU, RVV | RVV vector physics, software RT fallback |

Deliverables: GLSL RDNA compute kernels, BVH builder, SAP broadphase, SAT/GJK-EPA narrowphase, rigid body integrator.

---

### Universal AI Acceleration Layer

```rust
match BackendSelector::best_available() {
    Backend::AMD_XDNA         => xdna::run(model, input),
    Backend::RDNA_GPU         => rdna_ml::dispatch(model, input),
    Backend::NVIDIA_TENSOR    => tensor_rt::infer(model, input),
    Backend::INTEL_AI         => openvino::infer(model, input),
    Backend::ARM_NPU          => nnapi::infer(model, input),
    Backend::MooreThreads_GPU => musa_ml::dispatch(model, input),
    Backend::Kirin_NPU        => kirin_npu::infer(model, input),
    Backend::Kirin_GPU        => mali_vulkan_ml::dispatch(model, input),
    Backend::Samsung_RDNA     => xclipse_ml::dispatch(model, input),
    Backend::Samsung_NPU      => samsung_npu::infer(model, input),
    Backend::RiscV_NPU        => risc_v_npu::infer(model, input),
    Backend::RiscV_GPU        => vortex_ml::dispatch(model, input),
    Backend::RiscV_CPU        => rvv_simd::infer(model, input),
    Backend::CPU              => cpu_simd::infer(model, input),
}
```

---

### DirectX 12 Backend (Complete)

Implemented in `crates/dx12/` with full module coverage:

- DXGI swapchain, command queues, descriptor heaps (CBV/SRV/UAV/RTV/DSV)
- Root signatures, PSOs (Pipeline State Objects)
- DXR ray tracing (BLAS/TLAS/raygen/miss/hit)
- DXIL shader compilation
- DirectML backend for AI inference
- DX12 ? Vulkan translation layer (vkd3d-style)
- Steam Deck DX12 support

**Module breakdown:**
| Module | File | Purpose |
|--------|------|---------|
| instance | `instance.rs` | DXGI factory, adapter enumeration |
| device | `device.rs` | D3D12 device, command queues |
| swapchain | `swapchain.rs` | IDXGISwapChain4 management |
| command | `command.rs` | Allocators, lists, fences |
| descriptor | `descriptor.rs` | Descriptor heaps |
| pipeline | `pipeline.rs` | PSO creation (graphics/compute) |
| ray_tracing | `ray_tracing.rs` | DXR pipeline, acceleration structures |
| shader | `shader.rs` | DXIL compilation, root signatures |
| allocator | `allocator.rs` | Buffer/texture allocation |

```rust
// Backend selection (Windows: DX12 first, fallback to Vulkan)
let backend = select_backend()?;
println!("Using: {}", backend.name());
```

---

## Graphics API Status

| API | Tier | Status | Notes |
|-----|------|--------|-------|
| **Vulkan 1.3** | Higher | ? **Implemented** | Full backend in `crates/vulkan/`  VMA, RT pipeline, BLAS/TLAS, swapchain, command pools |
| **DX12** | Higher | ? **Implemented** | DXGI, DXR, descriptor heaps, PSOs, root signatures, acceleration structures |
| **AMD AGS** | Higher | ✅ **Implemented** | Real AMD AGS library with power management, fan control, performance profiling, thermal monitoring |
| **MUSA** | Lower | ?? **Planned** | Vendor detection in `fsr4_integration.rs` (ID `0x1DD`); native compute physics in roadmap |
| **NNAPI** | Lower | ?? **Planned** | Referenced as ARM NPU inference path; no implementation yet |
| **DirectML** | Lower | ?? **Planned** | Listed for NVIDIA Tensor Cores and Windows AI inference; no implementation yet |

**Implemented crates:**

| Crate | Path | APIs |
|-------|------|------|
| `litt-ags` | `crates/ags/src/` | AMD AGS bindings (power/fan/performance) |\n| `litt-vulkan` | `crates/vulkan/src/` | Vulkan 1.3 full backend |
| `litt-dx12` | `crates/dx12/src/` | DX12 + DXR + DirectML |
| `litt-fidelityfx` | `crates/fidelityfx/src/` | FSR 3/4, CAS, XESS 3, NPU vendor detection |
| `litt-ecs` | `crates/ecs/src/` | ECS core (World, Entity, Component, System) |
| `litt-platform` | `crates/platform/src/` | Windows (Win32), Linux (X11), Android (AAPI) |

See [docs/ROADMAP.md](./docs/ROADMAP.md) for detailed phase tracking.

---


## Architecture

```
Application Layer (main.rs)
    |  Camera, Player Controller, Scene Management, ECS World
    v
Platform Layer (litt-platform)
    |  Window creation, Input handling, Platform-specific code
    v
Vulkan Backend (litt-vulkan)  |  DX12 Backend (litt-dx12)
    |  VMA  Vulkan 1.3  RT   |  DXGI  D3D12  DXR  PSO
    v                          |     v
Renderer (litt-renderer)     ECS Systems (litt-ecs)
    |  Command Pools, Render   |  NeuralAI, Physics, Render, Input, UI
    |  Passes, Swapchain       |
    v                          |
Path Tracer (litt-pathtracer) |
    |  Raygen, CHIT, Miss,    |
    |  Russian Roulette       |
    v                          |
FidelityFX (litt-fidelityfx) |
    |  FSR 3.1.5, FSR 4, CAS, |
    |  Ray Reconstruction,    |
    |  Denoisers, XESS 3, NPU |
    v                          |
Display (Present) <------------+
```

See [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md) for full details and [litt-engine-architecture.html](./litt-engine-architecture.html) for an interactive visualization.

---


## Module Layout

```
litt/
  src/
    main.rs              # Entry point (Win32 / X11 / Android)
    lib.rs               # Public API surface, re-exports
    version.rs           # Semantic versioning
    ecs.rs               # ECS integration (systems, world setup)
    graphics.rs          # Graphics backend abstraction (Vulkan/DX12)
  crates/
    math/                # Vec2/3/4, Mat4, quaternions, SIMD
    platform/            # Window, input, platform abstraction
    vulkan/              # Vulkan backend (VMA, RT, swapchain)
    renderer/            # Command pools, render passes, descriptors
    pathtracer/          # BLAS/TLAS, ray tracing, BRDFs
    fidelityfx/          # FSR 3/4, CAS, denoisers, NPU
    ecs/                 # ECS core (World, Entity, Component, System)
    dx12/                # DX12 backend (DXGI, DXR, PSO, command)
  shaders/
    pathtracer/          # raygen, chit, miss (.glsl)
    fidelityfx/          # FSR, CAS, denoisers, XESS3 (.glsl)
    compute/             # tonemap, blur, TAA, atlas, splat, resolve
    mesh/                # vertex + fragment for mesh rendering
    quad/                # full-screen quad for post-process
  template/
    src/components/      # Camera, Player, Transform, Mesh, Material, Light
    agent/               # actions.log, PR_TEMPLATE.md
    assets/              # asset_index.json, ATTRIBUTION.md
  docs/
    ROADMAP.md           # Full 15-phase development roadmap
    ARCHITECTURE.md      # Architecture diagrams
    FSR_SUPPORT.md       # FSR version matrix
    NPU_SUPPORT.md       # NPU backend details
    DX12_SUPPORT.md      # DirectX 12 backend details
    ECS_ARCHITECTURE.md  # ECS API reference
    AMD_OPTIMIZATION.md  # RDNA-specific tuning
    MOORE_THREADS.md     # MUSA support
    INTEL_XESS3.md       # Intel AI Boost integration
    BINARY_SIZE.md       # Size optimization guides
  examples/
    basic_scene.rs       # Example scene
  Cargo.toml             # Workspace: 8 crates
```

---


## Quick Start

```bash
# Build with Vulkan (default)
cargo build --release

# Build with DX12 (Windows)
cargo build --release --features dx12

# Build with both backends (DX12 preferred on Windows)
cargo build --release --features dx12,vulkan

# Run
cargo run --release

# Environment variables
LIT_FSR_MODE=4        # Use FSR 3 (auto-select best available)
LIT_FSR_QUALITY=1     # Quality preset
LIT_NPU_MODE=3        # Hybrid NPU+GPU mode
LIT_GRAPHICS_API=dx12 # Force DX12 backend
```

---


## Cargo Workspace

| Crate | Dependencies | Purpose |
|-------|-------------|---------|
| `litt-math` | none | SIMD math types (Vec2/3/4, Mat4) |
| `litt-platform` | ash, bytemuck | Window + input abstraction |
| `litt-ags` | `crates/ags/src/` | AMD AGS bindings (power/fan/performance) |\n| `litt-vulkan` | ash, ash-window, vma, bytemuck, litt-math, litt-platform | Vulkan backend |
| `litt-renderer` | ash, bytemuck, litt-math, litt-vulkan | Render passes + command pool |
| `litt-pathtracer` | ash, bytemuck, litt-math, litt-vulkan, litt-renderer, vma | RT pipeline |
| `litt-fidelityfx` | ash, bytemuck, litt-math, litt-vulkan, vma | FSR 3/4, CAS, denoisers |
| `litt-ecs` | litt-math, bytemuck | ECS core (World, Entity, Component, System) |
| `litt-dx12` | bytemuck, litt-math, litt-platform | DX12 backend |

---


## Binary Size

| Phase | Windows | Linux | Android |
|-------|---------|-------|---------|
| Foundation | ~500 KB | ~400 KB | ~300 KB |
| Core Rendering | ~700 KB | ~600 KB | ~500 KB |
| FidelityFX | ~950 KB | ~850 KB | ~750 KB |
| **Target** | **< 1 MB** | **< 900 KB** | **< 800 KB** |

Optimisation flags: `opt-level = "z"`, `lto = true`, `codegen-units = 1`, `panic = "abort"`, `strip = true`.

---


## Roadmap

See [docs/ROADMAP.md](./docs/ROADMAP.md) for the full development plan including ECS architecture, physics system, DirectX 12 backend, asset pipeline, engine modules, networking, debug tools, game development, and NPU roadmap.

See [docs/NPU_RULES.md](./docs/NPU_RULES.md) for NPU system rules, core components, inference rules, and architecture reference.

| Phase | Title | Status |
|-------|-------|--------|
| 1 | Foundation | ? Complete |
| 2 | Core Rendering | ? Complete |
| 3 | FidelityFX & AI Upscaling | ? Complete |
| 4 | ECS Architecture | ? Complete |
| 5 | Physics System | ?? Planned |
| 6 | Universal AI Acceleration | ?? Planned |
| 7 | DirectX 12 Backend | ? Complete |
| 8 | Asset Pipeline | ?? Planned |
| 9 | Engine Modules | ?? Planned |
| 10 | Networking | ?? Planned |
| 11 | Platform Support | ?? Ongoing |
| 12 | Debug & Profiling | ?? Planned |
| 13 | Binary Size Verification | ? Complete |
| 14 | Polish | ?? In Progress |
| 15 | Planned Features | ?? Backlog |

---



---

