# Litt Engine Development Roadmap

---

## Phase 1: Foundation [COMPLETE]

- [x] Project structure
- [x] Custom math types (`crates/math`)
- [x] Platform abstraction layer (`crates/platform`)
- [x] Vulkan backend skeleton (`crates/vulkan`)
- [x] Path tracing shader stubs
- [x] FidelityFX integration points
- [x] Complete Vulkan device initialization
- [x] Implement swapchain management
- [x] Add command buffer recording
- [x] Implement descriptor set management
- [x] **Add VMA memory allocator integration** (vma crate)

---

## Phase 2: Core Rendering [COMPLETE]

- [x] Path tracer with BLAS/TLAS build pipeline
- [x] Complete ray tracing shaders (raygen, chit, miss)
- [x] Russian roulette termination
- [x] Temporal accumulation buffer
- [ ] **Add ReSTIR for light sampling**
- [x] Tonemapping pipeline
- [x] Mesh rendering pipeline (vert/frag shaders)
- [x] Quad overlay system
- [x] Render pass architecture
- [x] Command pool management

---

## Phase 3: FidelityFX & AI Upscaling [COMPLETE]

- [x] **Integrate FSR 3.1.5 with full compute shaders**
  - [x] FSR 3 create pass (temporal accumulation)
  - [x] FSR 3 compensate pass (motion vectors)
  - [x] FSR 3 upscaler pass
  - [x] FSR 3 frame generation pass
- [x] Add CAS sharpening
- [x] Implement Ray Reconstruction
- [x] Diffuse + Specular denoisers
- [x] **Add FSR 4 integration**
  - [x] FSR 4 upscaler compute shader
  - [x] FSR 4 framegen compute shader
- [x] Intel XESS 3 frame generation

---

## Phase 4: ECS Architecture [COMPLETE]

- [x] Pure ECS implementation (`crates/ecs`)
  - [x] Entities = IDs (u32)
  - [x] Components = plain data structs (blanket impl for Send + Sync + ''static)
  - [x] Systems = pure logic functions (trait with `update` and `name`)
  - [x] World = HashMap-based component storage
  - [x] Query API (`query_entities::<C>`, `query_entities_with::<C1, C2>`)
  - [x] SystemGroup for grouped system execution
- [x] Core component types
  - [x] `Transform` -- position, rotation, scale
  - [x] `NeuralBrain` -- AI model reference + state
  - [x] `BehaviorState` -- current behavior tree state
  - [x] `MovementIntent` -- desired velocity/direction
  - [x] `CombatIntent` -- target + action queue
  - [x] `Renderable` -- mesh handle + material ref
  - [x] `PhysicsBody` -- collider shape + mass + velocity
  - [x] `InputState` -- aggregated input per entity
  - [x] `Light` -- point/spot/directional light data
- [x] Core systems
  - [x] `NeuralAISystem` -- NPU-driven behavior inference
  - [x] `PhysicsSystem` -- GPU-accelerated rigid body simulation
  - [x] `RenderSystem` -- ECS → Vulkan/DX12 draw commands
  - [x] `InputSystem` -- keyboard/mouse/gamepad aggregation
  - [x] `UIOverlaySystem` -- HUD, menus, debug overlays
  - [x] `NetworkingSystem` (optional)
- [x] Query API
- [x] NPC behavior flow
- [x] `MovementSystem` -- updates transforms based on velocity
- [x] `CameraSystem` -- follows player entity
- [x] `LightSystem` -- animates light direction
- [x] `SystemGroup` -- grouped system execution with ordered scheduling

---

## Phase 5: Physics System [PLANNED]

### RDNA Tier
- [ ] GPU broadphase
- [ ] SIMD narrowphase
- [ ] Async compute integration
- [ ] RT physics queries
- [ ] BVH reuse
- [ ] Wave32 optimizations
- [ ] Subgroup operations

### ARM Tier
- [ ] NEON-accelerated physics
- [ ] Fixed-step simulation

### Moore Threads
- [ ] MUSA compute physics

### Kirin
- [ ] Mali compute physics
- [ ] NEON fallback

### Samsung RDNA
- [ ] RDNA compute physics

### RISC-V
- [ ] RVV vector physics
- [ ] Software RT fallback

### Deliverables
- [ ] GLSL RDNA compute kernels
- [ ] ECS physics integration
- [ ] BVH builder/rebuilder
- [ ] Broadphase (SAP/GPU-AABB)
- [ ] Narrowphase (SAT/GJK-EPA)
- [ ] Rigid body integrator

---

## Phase 6: Universal AI Acceleration Layer [PLANNED]

### Backend Selector
- [x] `BackendSelector::best_available()` auto-detection (graphics)
- [ ] AMD X DNA (RDNA + Ryzen AI NPU) -- NPU inference pipeline
- [x] RDNA GPU compute ML -- FSR 4 integration
- [ ] NVIDIA Tensor Cores (DirectML)
- [ ] Intel AI Boost / Arc XMX
- [ ] ARM NPUs (Qualcomm Hexagon, MediaTek APU)
- [ ] Kirin NPUs
- [ ] Samsung RDNA (Xclipse) + NPU
- [ ] RISC-V NPUs
- [ ] Vortex GPU (Skybox)
- [ ] Moore Threads MUSA ML
- [x] CPU fallback (AVX2, AVX-512, NEON, RVV)
- [x] Vulkan fallback (software RT)
- [ ] SwiftShader fallback

### NPU Inference
- [ ] AMD X DNA inference pipeline
- [ ] ARM NPU inference (NNAPI/Hexagon)
- [ ] Kirin NPU inference
- [ ] Samsung NPU inference
- [ ] Moore Threads ML dispatch
- [ ] RISC-V NPU inference
- [ ] NVIDIA Tensor RT inference
- [ ] Intel AI Boost inference
- [x] CPU SIMD fallback inference

---

## Phase 7: DirectX 12 Backend [COMPLETE]

- [x] DXGI swapchain
- [x] Command queues
- [x] Descriptor heaps (CBV/SRV/UAV/RTV/DSV)
- [x] Root signatures
- [x] PSOs (Pipeline State Objects)
- [x] DXIL shader compilation
- [x] DXR ray tracing (BLAS/TLAS/raygen/miss/hit)
- [x] DirectML backend for AI inference
- [x] DX12 compute fallback
- [x] DX12 → Vulkan translation layer (vkd3d-style)
- [x] Steam Deck DX12 support

**Implementation:** `crates/dx12/` with modules: instance, device, swapchain, command, descriptor, pipeline, ray_tracing, shader, allocator.

---

## Phase 8: Asset Pipeline [PLANNED]

- [ ] glTF 2.0 importer
- [ ] PNG/JPG/KTX2 texture loading
- [ ] WAV/OGG audio format support
- [ ] KTX2/BasisU transcoding pipeline
- [ ] SPIR-V shader compilation (build.rs)
- [ ] Hot-reload for shaders
- [ ] JSON/TOML metadata parsing
- [ ] Scene metadata import
- [ ] Asset index management (`template/assets/asset_index.json`)

---

## Phase 9: Engine Modules [PLANNED]

### UI Module
- [ ] Widget system
- [ ] Text rendering
- [ ] Button/input handling
- [ ] HUD overlay
- [ ] In-game console

### Scene Graph Module
- [ ] Node hierarchy
- [ ] Transform propagation
- [ ] Scene serialization
- [ ] Scene streaming

### Audio Module
- [ ] OpenAL/SDL2 audio backend
- [ ] 3D spatial audio
- [ ] Audio mixing
- [ ] Dynamic streaming

### Scripting Module
- [ ] Lua integration (or Rust API)
- [ ] Hot-reload scripts
- [ ] AI behavior scripting

### Controller Module
- [ ] Steam Deck controller support
- [ ] Gamepad abstraction
- [ ] Haptic feedback

---

## Phase 10: Networking [PLANNED]

- [ ] UDP networking
- [ ] WebSocket support
- [ ] ENet integration
- [ ] SteamNetworkingSockets
- [ ] Snapshot interpolation
- [ ] ECS entity replication
- [ ] Latency compensation
- [ ] Multiplayer room management

---

## Phase 11: Platform Support [ONGOING]

| Platform | Status | Details |
|----------|--------|---------|
| **Windows** | ✅ Implemented | DX12 (native) + Vulkan via ash |
| **Linux (Wayland + X11)** | 🔄 Partial | RADV driver |
| **Steam Deck** | 🔄 Partial | RADV + DX12 via Proton |
| **Android** | 🔄 Partial | Adreno, Mali, PowerVR |
| **RISC-V Linux** | 📋 Planned | RVV SIMD, Vortex GPU |
| **Moore Threads GPUs** | 📋 Planned | MUSA compute |
| **Kirin Devices** | 📋 Planned | Mali + NPU |
| **Samsung RDNA** | 📋 Planned | Xclipse GPU + NPU |

### Platform-Specific Optimizations
- [ ] Steam Deck: RADV tuning, controller overlay, power management
- [ ] Android: Vulkan validation layers, GPU driver quirks, battery optimization
- [ ] RISC-V: RVV vectorization, software fallback paths
- [x] Windows: DX12 best practices, DirectML routing

---

## Phase 12: Debug & Profiling [PLANNED]

- [ ] AMD RGP (Radeon GPU Profiler) integration
- [ ] GPU markers / debug regions
- [ ] Async compute zones
- [ ] RT pipeline markers
- [ ] FidelityFX profiling hooks
- [ ] Debug overlay tools
- [ ] ECS inspector (entity/component browser)
- [ ] GPU timing overlay (fps, ms, draw calls)
- [ ] Physics debug draw (AABBs, contacts, BVH)
- [ ] BVH visualization
- [ ] AI inference heatmap overlay
- [ ] Backend selection display
- [ ] Memory allocator tracking
- [ ] Shader hot-reload with error reporting

---

## Phase 13: Binary Size Verification [COMPLETE]

- [x] Dev builds target < 1 MB
- [x] Production builds may relax
- [x] Size-checking scripts included
- [x] `opt-level = "z"` + `lto = true` + `codegen-units = 1`
- [x] `panic = "abort"` + `strip = true` + `debug = false` + `rpath = false`

| Phase | Windows | Linux | Android |
|-------|---------|-------|---------|
| Phase 1 | ~500 KB | ~400 KB | ~300 KB |
| Phase 2 | ~700 KB | ~600 KB | ~500 KB |
| Phase 3 | ~850 KB | ~750 KB | ~650 KB |
| Phase 4 | ~950 KB | ~850 KB | ~750 KB |
| Phase 5 | < 1 MB | < 900 KB | < 800 KB |

---

## Phase 14: Polish [IN PROGRESS]

- [ ] **Add ReSTIR for light sampling** (path tracer)
- [ ] Binary size verification (< 1 MB) -- in progress
- [ ] AMD RGP profiling integration
- [ ] Memory leak detection
- [ ] Error handling improvements
- [ ] Documentation completion

---

## Phase 15: Planned Features [BACKLOG]

### Rendering
- [ ] **FSR 4.1 support** (RDNA 4/5)
- [ ] Extended FidelityFX denoisers
- [ ] DLSS/FSR hybrid upscaling

### AI Acceleration
- [ ] More NPU backends (emerging silicon)
- [ ] More mobile GPU backends
- [ ] ONNX model import pipeline
- [ ] Real-time neural animation blending

### Debug Tools
- [ ] Extended RGP integration
- [ ] GPU debugger protocol
- [ ] Live ECS component inspector
- [ ] Physics replay system

### Platform Support
- [ ] Extended Steam Deck support (LED, gyroscope, trackpad)
- [ ] WebGPU backend for browser deployment
- [ ] Console targets (if licensed)

---

## Folder Structure (Actual)

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

## Rust Skeleton (Actual)

```rust
// no_std compatible, minimal alloc
#![no_std]
#![cfg_attr(feature = "std", feature = "alloc_error_handler")]

// ECS world
use litt_ecs::{World, Entity, Component, System};

// Vulkan + DX12 backends (selected at runtime)
use litt::{GraphicsBackend, select_backend};

fn main() {
    let mut world = World::default();
    let backend = select_backend().expect("No graphics backend available");

    // Register ECS systems
    world.add_system(MovementSystem { dt: 0.016 });
    world.add_system(CameraSystem { dt: 0.016 });
    world.add_system(RenderSystem { backend: &backend });

    loop {
        backend.present();           // RenderSystem
        world.run_systems(0.016);     // All ECS systems
    }
}
```

---

## Game Development (Target Deliverables)

### Core Game Systems
- [ ] **Player controller** -- WASD/movement, camera, jump, interact
- [ ] **Camera** -- FPS/TPS modes, smooth follow, collision response
- [ ] **NPCs** -- spawner, pathfinding, state machine
- [ ] **Neural AI** -- behavior trees driven by NPU inference
- [ ] **UI** -- HUD, menus, settings panel, debug overlay
- [ ] **Audio** -- spatial audio, music, SFX mixing
- [ ] **Save/Load** -- ECS serialization, compression
- [ ] **Settings** -- graphics, audio, input, accessibility
- [ ] **Deployment** -- packaging scripts, CI/CD
- [ ] **Packaging** -- Steam, Android APK, Linux AppImage
- [ ] **Optimisation tips** -- RDNA, ARM, RISC-V specific
- [ ] **Steam Deck notes** -- controller mapping, power limits
- [ ] **Android notes** -- GPU driver quirks, thermal throttling
- [ ] **Linux notes** -- Wayland vs X11, RADV flags
- [ ] **Windows notes** -- DX12 best practices, WDK

---

## Estimated Timeline

| Phase | Title | Status | Estimate |
|-------|-------|--------|----------|
| 1 | Foundation | ✅ Complete | Done |
| 2 | Core Rendering | ✅ Complete | Done |
| 3 | FidelityFX & AI Upscaling | ✅ Complete | Done |
| 4 | ECS Architecture | ✅ Complete | Done |
| 5 | Physics System | 📋 Planned | 6-8 weeks |
| 6 | Universal AI Acceleration | 📋 Planned | 4-6 weeks |
| 7 | DirectX 12 Backend | ✅ Complete | Done |
| 8 | Asset Pipeline | 📋 Planned | 3-4 weeks |
| 9 | Engine Modules | 📋 Planned | 6-8 weeks |
| 10 | Networking | 📋 Planned | 4-6 weeks |
| 11 | Platform Support | 🔄 Ongoing | Continuous |
| 12 | Debug & Profiling | 📋 Planned | 3-4 weeks |
| 13 | Binary Size Verification | ✅ Complete | Done |
| 14 | Polish | 🔄 In Progress | 2-3 weeks |
| 15 | Planned Features | 📋 Backlog | TBD |

---



---

## NPU Roadmap [PLANNED]

### Phase NPU-1: NPU Core Infrastructure
- [ ] `NpuContext` global state management
- [ ] `NpuPipeline` compilation and loading
- [ ] `NpuTensor` pinned memory allocator
- [ ] Async inference queue with batching
- [ ] NPU telemetry system
- [ ] Error handling and recovery

### Phase NPU-2: NPC Neural AI
- [ ] `NeuralBrain` component full implementation
- [ ] `BehaviorState` NPU-driven state machine
- [ ] `EmotionalState` vector dynamics
- [ ] `NpcMemory` long-term memory system
- [ ] `NeuralAISystem` ECS integration
- [ ] Emergent behavior generation

### Phase NPU-3: Player Prediction
- [ ] `PlayerPredictor` component
- [ ] Movement pattern modeling
- [ ] Combat habit learning
- [ ] Exploration tendency tracking
- [ ] Adaptive difficulty adjustment

### Phase NPU-4: Procedural Generation
- [ ] `ProceduralGenerationSystem`
- [ ] Level/dungeon generation models
- [ ] Quest and puzzle generation
- [ ] Loot table optimization
- [ ] Enemy wave balancing

### Phase NPU-5: Dialogue & Personality
- [ ] `DialogueState` component
- [ ] LLM-like NPC dialogue models
- [ ] Personality shift system
- [ ] Emotional reaction modeling
- [ ] Memory-augmented conversations

### Phase NPU-6: Neural Animation
- [ ] `NeuralAnimationBlend` component
- [ ] Neural locomotion system
- [ ] Neural IK solver
- [ ] Gesture prediction
- [ ] Facial animation inference

### Phase NPU-7: Combat AI
- [ ] `CombatDecisionSystem`
- [ ] Attack selection models
- [ ] Dodge/defense prediction
- [ ] Target prioritization
- [ ] Group tactic generation

### Phase NPU-8: World Simulation
- [ ] `WorldSimulationSystem`
- [ ] Crowd behavior models
- [ ] Ecosystem simulation
- [ ] Faction AI networks
- [ ] Wildlife behavior trees

### Phase NPU-9: Advanced Systems
- [ ] `AudioAISystem` - voice synthesis
- [ ] `AntiCheatSystem` - anomaly detection
- [ ] `PhysicsPredictionSystem` - neural physics helpers
- [ ] `RlTrainingSystem` - online RL for NPCs
- [ ] `EngineOptimizerSystem` - AI-based LOD/scheduling

### Phase NPU-10: Cross-Platform NPU Support
- [ ] AMD X DNA inference pipeline
- [ ] Intel AI Boost inference
- [ ] Qualcomm Hexagon NPU
- [ ] Apple Neural Engine
- [ ] Kirin NPU
- [ ] Samsung Exynos NPU
- [ ] RISC-V AI accelerators


*Last updated: 2026-07-18*