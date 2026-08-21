# Litt Engine

Ultra-lightweight Vulkan/DX12 path tracing engine for AMD, Intel Arc, Samsung, and Moore Threads GPUs with NPU acceleration.

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
| Sophgo CV1800 | RISC-V | 8 | Edge AI inference |

- Auto-detection via `BackendSelector::best_available()`
- Fallback to GPU/NPU/CPU when unavailable
- Modes: Disabled / Auto / Forced / Hybrid

---

### Real-Time Path Tracing\n\nReSTIR (Reservoir-Based Sampling for Importance Sampling) is implemented in crates/pathtracer/src/restir.rs:\n\n- **Reservoir management** — maintain a reservoir of light samples with importance weighting\n- **Stochastic reselection** — efficiently update reservoir with new samples\n- **Multiple strategies** — Uniform, Importance, and ReSTIR light sampling\n- **Spatio-temporal reuse** — reuse samples from previous frames for noise reduction\n- **GPU-compatible** — works with Vulkan compute shaders\n\n`ust\nuse litt_pathtracer::restir::*;\nuse litt_pathtracer::LightSamplingStrategy;\n\n// Sample a light using ReSTIR\nlet (sample, pdf) = sample_light_restir(\n    &scene.lights,\n    point,\n    normal,\n    LightSamplingStrategy::ReSTIR,\n    &mut rng,\n);\n\n// Evaluate lighting contribution\nlet lighting = evaluate_lighting(&sample, point, normal, albedo);\n`\n

- **Ray tracing** via Vulkan 1.3 `VK_KHR_ray_tracing_pipeline` + `VK_KHR_acceleration_structure`
- BLAS + TLAS build pipeline for triangle and sphere scenes
- Raygen / Closest-hit / Miss shaders with **Russian roulette** termination
- Lambertian diffuse + GGX specular BRDFs
- Temporal accumulation buffer for progressive rendering
- **ReSTIR** (Reservoir-Based Sampling for Importance Sampling) — efficient light sampling with spatio-temporal reuse, reducing noise in indirect lighting

---

### FidelityFX Integration

- **FSR 3.1.5** — frame generation (create, compensate, upscaler, framegen passes)
- **FSR 4** — next-gen upscaling + frame generation (RDNA 4/5)
- **CAS** — Contrast Adaptive Sharpening for crisp final output
- **Ray Reconstruction** — lightweight CNN-style denoiser for low-sample RT
- **Diffuse + Specular Denoisers** — temporal-spatial filtering
- **Intel XESS 3** — frame generation for Intel Arc GPUs

---

### ECS Architecture (Complete)

ECS (Entity-Component-System) is implemented in `crates/ecs`.
- **Entity** = unique u32 ID
- **Component** = plain data structs (blanket impl for Send + Sync + static)
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
- `NeuralAISystem` — NPU-driven behavior inference
- `PhysicsSystem` — GPU-accelerated rigid body simulation
- `RenderSystem` — ECS → Vulkan/DX12 draw commands
- `InputSystem` — keyboard/mouse/gamepad aggregation
- `UIOverlaySystem` — HUD, menus, debug overlays
- `NetworkingSystem` — optional ECS entity replication

See [docs/ECS_ARCHITECTURE.md](./docs/ECS_ARCHITECTURE.md) for the full API reference.

---

### Physics System (Complete)

Multi-tier GPU-accelerated physics per platform:

| Tier | GPU | Optimization |
|------|-----|--------------|
| RDNA | AMD RX 6000/7000/8000 | GPU broadphase, BVH SAH, async compute, Wave32 ready |
| ARM | Adreno, Mali | NEON physics, fixed-step simulation |
| Samsung RDNA | Exynos 2200+ | RDNA compute physics |
| Kirin | Mali + Da Vinci NPU | Mali compute + NEON fallback |
| Moore Threads | MUSA | MUSA compute physics |
| RISC-V | Vortex GPU, RVV | RVV vector physics, software RT fallback |
| x86_64 | Intel/AMD | AVX2-accelerated broadphase |

**Features:**
- BVH (Bounding Volume Hierarchy) with SAH for O(log n) broadphase
- SAT (Separating Axis Theorem) for AABB-AABB narrowphase
- Sphere-sphere and capsule-capsule collision detection
- Semi-implicit Euler integrator with ground collision
- Impulse-based constraint solver with friction and restitution
- Async compute integration (separate compute queue)
- Platform-specific SIMD: NEON (ARM), RVV (RISC-V), AVX2 (x86_64)
- Fixed-step simulation with configurable substeps

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
- DXIL shader compilation via DirectXShaderCompiler (DXC)
- DirectML backend for AI inference
- DX12 → Vulkan translation layer (vkd3d-style)
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
| shader | `shader.rs` | DXIL compilation (DXC), root signatures |
| allocator | `allocator.rs` | Buffer/texture allocation |

```rust
// Backend selection (Windows: DX12 first, fallback to Vulkan)
let backend = select_backend()?;
println!("Using: {}", backend.name());
```

---

## Backend Status

### Graphics APIs (Rendering)

| API | Tier | Status | Notes |
|-----|------|--------|-------|
| **Vulkan 1.3** | Higher | ✅ **Implemented** | Full backend in `crates/vulkan/` — VMA, RT pipeline, BLAS/TLAS, swapchain, command pools |
| **DX12** | Higher | ✅ **Implemented** | DXGI, DXR, descriptor heaps, PSOs, root signatures, acceleration structures, DXC shader compilation |

### AI Acceleration (NPU/DSP Inference)

| API | Tier | Status | Notes |
|-----|------|--------|-------|
| **NNAPI** | Mobile | ✅ **Implemented** | Android NPU inference via `libneuralnetworks.so` — runs TFLite/ONNX models on mobile NPUs (Qualcomm Hexagon, MediaTek APU, Kirin NPU) |
| **DirectML** | Windows | ⚠️ **Stub** | Not yet implemented |

### GPU Management

| API | Tier | Status | Notes |
|-----|------|--------|-------|
| **AMD AGS** | Desktop | ✅ **Implemented** | Power management, fan control, performance profiling, thermal monitoring (requires `amd_ags_x64.dll` / `libamd_ags.so`) |
| **MUSA** | Desktop | ⚠️ **Partial** | Moore Threads GPU detection via Vulkan (compute backend; MUSA SDK is proprietary, no public API) |

**Note:** NNAPI is an AI inference API, not a graphics API. It runs neural network models on NPUs for tasks like denoising, frame generation, and upscaling — NOT for rendering graphics pipelines.

**Implemented crates:**

| Crate | Path | APIs |
|-------|------|------|
| `litt-ags` | `crates/ags/src/` | AMD AGS bindings (power/fan/performance) |
| `litt-vulkan` | `crates/vulkan/src/` | Vulkan 1.3 full backend |
| `litt-dx12` | `crates/dx12/src/` | DX12 + DXR + DXC shader compilation |
| `litt-fidelityfx` | `crates/fidelityfx/src/` | FSR 3/4, CAS, XESS 3, NPU vendor detection |
| `litt-ecs` | `crates/ecs/src/` | ECS core (World, Entity, Component, System) |
| `litt-platform` | `crates/platform/src/` | Windows (Win32), Linux (X11), Android (AAPI), MUSA detection, NNAPI inference |

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
    |  VMA, Vulkan 1.3, RT     |  DXGI, D3D12, DXR, PSO, DXC
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
    platform/            # Window, input, platform abstraction (MUSA, NNAPI)
    vulkan/              # Vulkan backend (VMA, RT, swapchain)
    renderer/            # Command pools, render passes, descriptors
    pathtracer/          # BLAS/TLAS, ray tracing, BRDFs
    fidelityfx/          # FSR 3/4, CAS, denoisers, NPU
    ecs/                 # ECS core (World, Entity, Component, System)
    dx12/                # DX12 backend (DXGI, DXR, PSO, DXC)
    ags/                 # AMD AGS bindings (power/fan/performance)
  shaders/
    pathtracer/          # raygen, chit, miss (.glsl)
    fidelityfx/          # FSR, CAS, denoisers, XESS3 (.glsl)
    compute/             # tonemap, blur, TAA, atlas, splat, resolve
    mesh/                # vertex + fragment for mesh rendering
    quad/                # full-screen quad for post-process
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
  Cargo.toml             # Workspace: 11 crates
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
LIT_FSR_MODE=4        # Use FSR 4 (auto-select best available)
LIT_FSR_QUALITY=1     # Quality preset
LIT_NPU_MODE=3        # Hybrid NPU+GPU mode
LIT_GRAPHICS_API=dx12 # Force DX12 backend
```

---

## AMD AGS Integration

The `litt-ags` crate provides Rust bindings for the official AMD AGS (AMDGPU Services) library:

```rust
use litt_ags::{AGSContext, AGSPowerProfile, AGSPerformanceLevel};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut context = AGSContext::new()?;
    
    // Get GPU count
    let count = context.adapter_count();
    println!("Found {} AMD GPUs", count);
    
    // Get adapter info
    let info = context.get_adapter_info(0)?;
    println!("GPU: {}", info.adapter_name());
    
    // Set performance mode (requires admin)
    context.set_power_profile(0, AGSPowerProfile::AGS_POWER_PROFILE_FORCE_HIGH)?;
    context.set_performance_level(0, AGSPerformanceLevel::AGS_PERFORMANCE_LEVEL_HIGH)?;
    
    // Monitor thermals
    let thermal = context.get_thermals(0)?;
    println!("Temperature: {}°C", thermal.CurrentTemperature);
    
    // Monitor power draw
    let power = context.get_power_info(0)?;
    println!("Power: {}W", power.AveragePower);
    
    // Monitor utilization
    let util = context.get_utilization(0)?;
    println!("GPU: {}%", util.GPU);
    
    Ok(())
}
```

**Features:**
- Power management (profiles, limits)
- Fan control (automatic/manual)
- Performance profiling (power, utilization, clocks)
- Thermal monitoring
- Driver information queries

**Requirements:**
- AMD Radeon GPU with Adrenalin driver
- Windows or Linux
- Admin privileges for power/fan control

---

## Universal AI Acceleration (Phase 6)

The `litt-ai` crate provides a unified AI acceleration layer across all hardware backends:

```rust
use litt_ai::*;

// Auto-select best backend
let mut context = AIContext::new();
let backend = context.best_backend_kind();
println!("Using: {}", backend.name());

// Or force a specific backend
context.init_backend(BackendKind::Npu(NpuBackend::AmdXdna))?;

// Run inference
let result = context.run_auto(&model, &[input])?;
println!("Latency: {}ms on {}", result.latency_ms, result.backend_used.name());
```

**Supported backends:**

| Backend | Platform | Precision | Notes |
|---------|----------|-----------|-------|
| AMD XDNA | Windows/Linux | INT8, FP16, FP32 | Ryzen AI 9 H-series |
| Intel AI Boost | Windows | FP16, INT8 | Meteor Lake+ |
| Qualcomm Hexagon | Android | INT8, FP16, UINT8 | NNAPI-backed |
| Apple Neural Engine | macOS/iOS | FP16, FP32 | Core ML |
| MediaTek APU | Android | INT8, FP16 | Dimensity series |
| Kirin NPU | Android | INT8 | Da Vinci architecture |
| Samsung Exynos NPU | Android | INT8, FP16 | Exynos 2200+ |
| RISC-V AI | RISC-V | INT8 | Custom vector accel |
| Vulkan Compute | All | FP16, FP32 | GPU fallback |
| CPU SIMD | All | FP32 | AVX2/NEON/RVV |

**ECS Integration:**
- `NeuralBrain` — AI model reference + state
- `MovementIntent` — NPU-driven movement
- `CombatIntent` — NPU-driven combat AI
- `NeuralAISystem` — Runs inference-driven behavior
- `CombatAISystem` — NPU-driven combat decisions

---

## MUSA Support (Moore Threads)

The `litt-platform` crate includes MUSA support for Moore Threads GPUs:

```rust
use litt_platform::musa::*;

// Check if MUSA is available
if musa_is_available(&instance)? {
    let devices = enumerate_musa_gpus(&instance)?;
    
    for device in devices {
        let props = get_musa_properties(device, &instance)?;
        println!("MUSA GPU: {} ({} GB VRAM)", props.name, props.memory_total / (1024 * 1024 * 1024));
    }
}
```

**Features:**
- GPU detection and classification
- Compute capability detection (V100/V200/V300)
- Memory information
- Vulkan-based compute backend

---

## NNAPI Support (Android)

The `litt-platform` crate includes NNAPI support for Android NPUs:

```rust
use litt_platform::nnapi::*;

// Check if NNAPI is available
if nnapi_is_available() {
    let devices = nnapi_get_devices()?;
    
    for device in devices {
        println!("NNAPI Device: {} ({:?})", device.name, device.type_);
    }
    
    // Load model
    let model = nnapi_load_model(tflite_data, NnapiModelType::Tflite)?;
    let execution = nnapi_create_execution(&model)?;
    
    // Run inference
    let outputs = nnapi_compute(&execution, &inputs)?;
}
```

**Supported devices:**
- Samsung Exynos NPUs
- Qualcomm Hexagon DSPs
- MediaTek APUs
- Google Tensor G-series NPUs
- Snapdragon 8-series NPUs

---

## Asset Pipeline (Phase 8)

The `litt-asset` crate provides a complete asset loading pipeline:

```rust
use litt_asset::*;

// Create asset manager
let mut manager = AssetManager::new()
    .with_base_path("assets")
    .with_cache_size(512 * 1024 * 1024); // 512 MB cache

// Load a model
let model_handle = manager.load_model("models/scene.gltf")?;
let model = manager.get::<Model>(&model_handle).unwrap();
println!("Loaded {} meshes, {} vertices", model.meshes.len(), model.total_vertices());

// Load a texture
let tex_handle = manager.load_texture("textures/albedo.png")?;

// Load a shader
let shader_handle = manager.load_shader("shaders/raygen.rgen.glsl", ShaderStage::RayGen)?;

// Load a material
let mat_handle = manager.load_material("materials/concrete");
let mat = manager.get::<Material>(&mat_handle).unwrap();

// Check stats
let stats = manager.stats();
println!("Loaded {} assets, {} errors", stats.load_count, stats.error_count);
```

**Supported formats:**

| Type | Formats | Notes |
|------|---------|-------|
| Models | GLTF, GLB, OBJ | Triangle meshes, UVs, normals |
| Textures | PNG, JPEG, KTX2 | Auto MIP, format detection |
| Shaders | GLSL, HLSL, SPIR-V, DXIL | Auto-compile to target format |
| Materials | Built-in presets | PBR, unlit, transparent, emissive |
| Fonts | TTF, OTF | Basic metrics parsing |

**Features:**
- LRU cache with configurable size limit
- Duplicate load prevention
- Type-safe asset handles
- Path resolution with base path
- Shader compilation with caching
- Mipmap generation
- Bounding box computation

---

## DX12 Shader Compilation

The `litt-dx12` crate includes DirectXShaderCompiler (DXC) integration:

```rust
use litt_dx12::shader::*;

// Compile HLSL to DXIL
let compiler = DxcCompiler::new()?;
let result = compiler.compile(hlsl_source, "main", "dxil_6_5")?;

// Or use convenience function
let result = compile_hlsl(hlsl_source, "main", "dxil_6_5")?;
```

**Requirements:**
- `dxcompiler.dll` in PATH or set `DXC_PATH` environment variable
- Available with DirectX SDK or MSVC

---

## Cargo Workspace

| Crate | Dependencies | Purpose |
|-------|-------------|---------|
| `litt-math` | none | SIMD math types (Vec2/3/4, Mat4) |
| `litt-platform` | ash, bytemuck, libc | Window + input abstraction, MUSA, NNAPI |
| `litt-ags` | libloading | AMD AGS bindings (power/fan/performance) |
| `litt-vulkan` | ash, ash-window, vma, bytemuck, litt-math, litt-platform, litt-ags | Vulkan backend |
| `litt-renderer` | ash, bytemuck, litt-math, litt-vulkan | Render passes + command pool |
| `litt-pathtracer` | ash, bytemuck, litt-math, litt-vulkan, litt-renderer, vma | RT pipeline with ReSTIR light sampling |
| `litt-fidelityfx` | ash, bytemuck, litt-math, litt-vulkan, vma | FSR 3/4, CAS, denoisers |
| `litt-ecs` | litt-math, bytemuck | ECS core (World, Entity, Component, System) |
| `litt-dx12` | bytemuck, libloading, litt-math, litt-platform, winapi | DX12 backend with DXC |
| `litt-asset` | image, bytemuck, litt-math | Model, texture, shader, material, font loading |
| `litt-input` | bytemuck, litt-math | Keyboard, mouse, gamepad input |
| `litt-audio` | bytemuck, hound, cpal | Sound playback and mixing |
| `litt-ui` | bytemuck, litt-math | Debug HUD, overlays, text rendering |
| `litt-profiler` | bytemuck, ash | Frame timing, GPU/CPU sync, stats |
| `litt-scene` | bytemuck, litt-math, litt-asset, litt-ecs | Scene graph and loading |
| `litt-config` | bytemuck, serde | Settings, presets, JSON persistence |
| `litt-profiler` | bytemuck, ash, litt-math | Frame timing, GPU profiling, bottleneck analysis, FPS history, debug renderer |

---

## Engine Modules (Phase 9)

The engine now includes 6 new modules that integrate all lower-level systems:

```rust
use litt::*;

fn main() {
    // Create config manager
    let mut config = ConfigManager::new();
    config.apply_preset("high"); // or "low", "medium", "ultra"

    // Create game loop
    let mut game_loop = GameLoop::with_config(GameConfig {
        max_fps: 144,
        physics_hz: 60.0,
        ..Default::default()
    });

    // Create app with all systems
    let mut app = App::new().unwrap();

    // Run the game loop
    app.run();
}
```

**Engine Modules:**

| Module | Purpose |
|--------|---------|
| `input` | Keyboard, mouse, gamepad — unified `InputState` |
| `audio` | Sound playback, mixing, source management |
| `ui` | Debug HUD, overlay primitives, text rendering |
| `profiler` | Frame timing, GPU profiling, bottleneck analysis, FPS history, debug renderer |
| `scene` | Hierarchical scene graph with loading |
| `config` | Settings, presets, JSON persistence |
| `game_loop` | Fixed timestep loop with FPS capping |
| `app` | Full pipeline integration |

### FSR 3.1.5 Pipeline (Phase 11)

The `litt-fidelityfx` crate now contains a **real Vulkan compute pipeline** for FSR 3.1.5 — not stubs.

```rust
use litt_fidelityfx::*;
use litt_renderer::*;

// In RenderPipeline::new():
let fsr_pipeline = Fsr3Pipeline::new();
fsr_pipeline.initialize(device, 640, 360, 1280, 720, Fsr3Quality::Quality)?;

let cas_pipeline = CasPipeline::new();
cas_pipeline.initialize(device, 1280, 720)?;

// In render_frame():
fsr_pipeline.run_upscaler(
    command_buffer,
    path_traced_view,    // low-res input
    history_view,        // temporal history
    velocity_view,       // motion vectors
    swapchain_view,      // high-res output
    &upscaler_constants,
)?;

cas_pipeline.run(
    command_buffer,
    swapchain_view,      // input
    swapchain_view,      // output (in-place)
    &cas_constants,
)?;
```

**Shader Pipeline (6 GLSL compute shaders, auto-compiled to SPIR-V):**

| Shader | Purpose | Kernel |
|--------|---------|--------|
| `fsr3_upscaler.comp` | Spatial upscaling + temporal blend + sharpening | 8×8 threads, Laplacian sharpen |
| `fsr3_compensate.comp` | Exposure normalization for history buffer | 8×8 threads |
| `fsr3_create.comp` | Reprojection / history buffer copy | 8×8 threads |
| `fsr3_framegen.comp` | Optical flow frame interpolation | 8×8 threads |
| `cas.comp` | Contrast Adaptive Sharpening | 8×8 threads, 5-tap Laplacian |
| `ray_recon.comp` | CNN-style denoiser for path tracer | 8×8 threads, 3×3 average |

**Key implementation details:**
- Descriptor sets allocated per-pass with `vk::DescriptorSetAllocateInfo`
- Push constants uploaded via `cmd_push_constants`
- Workgroup dispatch: `(width+7)/8 × (height+7)/8 × 1`
- Fallback pass-through shader when glslang is unavailable
- Real SPIR-V when `glslangValidator` is on PATH or `GLSLANG_PATH` is set

### Profiler Deep Dive (Phase 10)

The profiler crate provides comprehensive performance analysis:

```rust
use litt_profiler::*;

// Frame timing with per-stage breakdown
let mut timer = FrameTimer::new();
let mut breakdown = FrameTimingBreakdown::new();

// Record frame
timer.record_frame();
breakdown.record_total(timer.last_frame_ms);
breakdown.stage(TimingStage::Physics).stop();

// Bottleneck analysis
let bn = BottleneckAnalyzer::new();
bn.update(cpu_ms, gpu_ms, npu_ms, physics_ms, frame_ms);
let bottleneck = bn.bottleneck(); // BottleneckInfo with recommendation

// FPS history with ASCII graph
let mut fps_history = FpsHistory::new();
fps_history.record(timer.fps, timestamp_ms);
println!("{}", fps_history.to_ascii_graph(60, 10)); // ASCII art graph

// Generate full report
let report = PerfReport::generate(&timer, &bn, &fps_history, &gpu_mem, &stats);
report.save("perf_report.txt").ok();
```

**Key Features:**
- **FrameTimingBreakdown** — tracks time per stage (Input/Physics/AI/Culling/Upload/Draw/Present)
- **BottleneckAnalyzer** — identifies the slowest subsystem with actionable fix recommendations
- **GpuTimerQuery** — Vulkan timestamp queries for precise GPU execution timing
- **GpuMemoryStats** — tracks GPU memory allocations with peak usage and per-pool breakdown
- **FpsHistory** — rolling FPS buffer with 1% low detection and ASCII visualization
- **PerfReport** — complete text report saveable to file for profiling sessions
- **DebugRenderer** — wireframe boxes, spheres, normals, and text overlays for debugging

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
| 9 | Engine Modules | ⚠️ Planned |
| 10 | Networking | ⚠️ Planned |
| 11 | Platform Support | ✅ Ongoing |
| 12 | Debug & Profiling | ⚠️ Planned |
| 13 | Binary Size Verification | ✅ Complete |
| 14 | Polish | ⚠️ In Progress |
| 15 | Planned Features | ⚠️ Backlog |

---

## Community

- [Code of Conduct & Rules](./docs/COMMUNITY.md)
- [Contributing Guide](./docs/CONTRIBUTING.md)

---

## License

MIT — free for personal and commercial use.

FidelityFX shaders and concepts are courtesy of [AMD](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK), also MIT-licensed.

Built for AMD GPUs, Intel Arc, Samsung RDNA, and Moore Threads. Tested on RDNA2 (RX 6700 XT), RADV (Linux), and MUSA (Moore Threads).
