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

## Phase 4: ECS Architecture [IN PROGRESS]

### ECS Core
- [ ] Pure ECS implementation
  - [ ] Entities = IDs
  - [ ] Components = plain data structs
  - [ ] Systems = pure logic functions
  - [ ] World = structure-of-arrays (SoA)
  - [ ] Zero-cost iteration, no dynamic dispatch in hot paths
- [ ] Core component types
  - [ ] `Transform` -- position, rotation, scale
  - [ ] `NeuralBrain` -- AI model reference + state
  - [ ] `BehaviorState` -- current behavior tree state
  - [ ] `MovementIntent` -- desired velocity/direction
  - [ ] `CombatIntent` -- target + action queue
  - [ ] `Renderable` -- mesh handle + material ref
  - [ ] `PhysicsBody` -- collider shape + mass + velocity
  - [ ] `InputState` -- aggregated input per entity
  - [ ] `Light` -- point/spot/directional light data
- [ ] Core systems
  - [ ] `NeuralAISystem`
  - [ ] `PhysicsSystem`
  - [ ] `RenderSystem`
  - [ ] `InputSystem`
  - [ ] `UIOverlaySystem`
  - [ ] `NetworkingSystem` (optional)
- [ ] Query API
- [ ] NPC behavior flow

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
- [ ] `BackendSelector::best_available()` auto-detection
- [ ] AMD XDNA (RDNA + Ryzen AI NPU)
- [ ] RDNA GPU compute ML
- [ ] NVIDIA Tensor Cores (DirectML)
- [ ] Intel AI Boost / Arc XMX
- [ ] ARM NPUs (Qualcomm Hexagon, MediaTek APU)
- [ ] Kirin NPUs
- [ ] Samsung RDNA (Xclipse) + NPU
- [ ] RISC-V NPUs
- [ ] Vortex GPU (Skybox)
- [ ] Moore Threads MUSA ML
- [ ] CPU fallback (AVX2, AVX-512, NEON, RVV)
- [ ] Vulkan fallback (software RT)
- [ ] SwiftShader fallback

### NPU Inference
- [ ] AMD XDNA inference pipeline
- [ ] ARM NPU inference (NNAPI/Hexagon)
- [ ] Kirin NPU inference
- [ ] Samsung NPU inference
- [ ] Moore Threads ML dispatch
- [ ] RISC-V NPU inference
- [ ] NVIDIA Tensor RT inference
- [ ] Intel AI Boost inference
- [ ] CPU SIMD fallback inference

---

## Phase 7: DirectX 12 Backend [PLANNED]

- [ ] DXGI swapchain
- [ ] Command queues
- [ ] Descriptor heaps
- [ ] Root signatures
- [ ] PSOs (Pipeline State Objects)
- [ ] DXIL shader compilation
- [ ] DXR ray tracing (BLAS/TLAS/raygen/miss/hit)
- [ ] DirectML backend for AI inference
- [ ] DX12 compute fallback
- [ ] DX12 -> Vulkan translation layer (vkd3d-style)
- [ ] Steam Deck DX12 support

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
| **Windows** | Partial | DX12 + Vulkan via ash |
| **Linux (Wayland + X11)** | Partial | RADV driver |
| **Steam Deck** | Partial | RADV + DX12 via Proton |
| **Android** | Partial | Adreno, Mali, PowerVR |
| **RISC-V Linux** | Planned | RVV SIMD, Vortex GPU |
| **Moore Threads GPUs** | Planned | MUSA compute |
| **Kirin Devices** | Planned | Mali + NPU |
| **Samsung RDNA** | Planned | Xclipse GPU + NPU |

### Platform-Specific Optimizations
- [ ] Steam Deck: RADV tuning, controller overlay, power management
- [ ] Android: Vulkan validation layers, GPU driver quirks, battery optimization
- [ ] RISC-V: RVV vectorization, software fallback paths
- [ ] Windows: DX12 best practices, DirectML routing

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

## Folder Structure (Target)

```
engine/
├── core/
│   ├── ecs/
│   │   ├── world.rs          # SoA world
│   │   ├── entity.rs         # Entity IDs
│   │   ├── component.rs      # Component registry
│   │   └── query.rs          # Query API
│   ├── math/                 # Already in crates/math
│   ├── platform/             # Already in crates/platform
│   └── utils/
├── renderer/
│   ├── vulkan/
│   │   ├── rt/
│   │   │   └── shaders/      # GLSL RT shaders
│   │   ├── instance.rs
│   │   ├── device.rs
│   │   ├── swapchain.rs
│   │   ├── pipeline.rs
│   │   ├── ray_tracing.rs
│   │   └── allocator.rs      # (VMA)
│   ├── dx12/                 # Planned
│   └── fidelityfx/           # Already in crates/fidelityfx
├── ai/
│   ├── npu/                  # Planned backends
│   └── inference.rs          # Planned
├── physics/                  # Planned
├── assets/
│   ├── models/               # Planned
│   ├── textures/             # Planned
│   ├── audio/                # Planned
│   └── shaders/              # Already present
├── modules/
│   ├── ui/                   # Planned
│   ├── scene_graph/          # Planned
│   ├── audio/                # Planned
│   └── scripting/            # Planned
├── networking/               # Planned
└── game/
    ├── main.rs               # Entry point
    ├── startup.rs            # Planned
    └── gameplay/             # Planned
```

---

## Rust Skeleton (Target)

```rust
// no_std compatible, minimal alloc
#![no_std]
#![cfg_attr(feature = "std", feature = "alloc_error_handler")]

// ECS world
use litt_ecs::{World, Entity, Component};

// Vulkan + DX12 backends (switched at compile time)
#[cfg(feature = "vulkan")]
use litt_vulkan::VulkanBackend;
#[cfg(feature = "dx12")]
use litt_dx12::Dx12Backend;

fn main() {
    let mut world = World::default();
    let backend = BackendSelector::best_available();
    
    loop {
        input::poll(&mut world);      // InputSystem
        ai::update(&mut world);       // NeuralAISystem
        physics::step(&mut world);    // PhysicsSystem
        render::frame(&mut world, &backend);  // RenderSystem + FidelityFX
        ui::overlay(&mut world);      // UIOverlaySystem
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
| 1 | Foundation | Complete | Done |
| 2 | Core Rendering | Complete | Done |
| 3 | FidelityFX & AI Upscaling | Complete | Done |
| 4 | ECS Architecture | In Progress | 4-6 weeks |
| 5 | Physics System | Planned | 6-8 weeks |
| 6 | Universal AI Acceleration | Planned | 4-6 weeks |
| 7 | DirectX 12 Backend | Planned | 4-6 weeks |
| 8 | Asset Pipeline | Planned | 3-4 weeks |
| 9 | Engine Modules | Planned | 6-8 weeks |
| 10 | Networking | Planned | 4-6 weeks |
| 11 | Platform Support | Ongoing | Continuous |
| 12 | Debug & Profiling | Planned | 3-4 weeks |
| 13 | Binary Size Verification | Complete | Done |
| 14 | Polish | In Progress | 2-3 weeks |
| 15 | Planned Features | Backlog | TBD |

---

*Last updated: 2025-07-18*
