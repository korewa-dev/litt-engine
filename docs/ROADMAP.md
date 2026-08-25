<!-- REMOVED STACK NOTICE (CDR-007): The Rust engine described here was removed from the repo; this document remains as design reference for the C/C++ port (native/littcore). -->
# Litt Engine -- Roadmap

A tiny feature-rich game engine with DX12/Vulkan support for AMD RDNA,
Intel Arc, Moore Threads MUSA, and NPU-equipped devices.

## Status Snapshot

| Area | State |
| --- | --- |
| Workspace build | removed with the Rust stack (CDR-007); C/C++ core under native/ |
| Vulkan backend | Real compute + RT pipelines, hand-rolled GpuAllocator |
| GAL translation layer | Logical device + command replay (Phase 17, tests green) |
| DX12 backend | Interface-complete stub hub (COM backend pending) |
| FSR 3.1.5 / CAS / Ray Reconstruction | Compute shaders + pipelines wired |
| Physics | CPU solver complete; RDNA Wave32/Subgroup/RT/BVH tiers ready |
| Networking | UDP/TCP transport, snapshots, replication (Phase 18, tests green) |
| Agent systems | Scene JSON, animation player, screenshots, replay, RL API (Phase 18, tests green) |

## Phase Index

1. Phase 1-4: Foundation, Rendering Core, ECS Gameplay Layer [COMPLETE]
2. Phase 5: Physics System [COMPLETE]
3. Phase 6: Universal AI Acceleration Layer [COMPLETE]
4. Phase 7: DirectX 12 Backend [STUB HUB]
5. Phase 8: Asset Pipeline [COMPLETE]
6. Phase 9: Engine Modules [COMPLETE]
7. Phase 10: Debug & Profiling [COMPLETE]
8. Phase 11: FSR 3.1.5 Real Pipeline [COMPLETE]
9. Phase 12: GPU Path Tracer [COMPLETE]
10. Phase 13: Path Tracer Bug Fixes + Layout Transitions [COMPLETE]
11. Phase 14: MUSA Complete Compute Pipeline [COMPLETE]
12. Phase 15: RDNA Tier [COMPLETE]
13. Phase 16: Comprehensive Engine Architecture [COMPLETE]
14. Phase 17: Graphics Abstraction Layer (GAL) [COMPLETE]
15. Phase 18: Networking & Agent Systems [COMPLETE]

---

## Phase 4: ECS Gameplay Layer [COMPLETE]

### ECS Components
   - [x] `InputState` -- aggregated input per entity
   - [x] `Light` -- point/spot/directional light data
- [x] Core systems
  - [x] `NeuralAISystem` -- NPU-driven behavior inference
  - [x] `PhysicsSystem` -- GPU-accelerated rigid body simulation
  - [x] `RenderSystem` -- ECS  Vulkan/DX12 draw commands
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

## Phase 5: Physics System [COMPLETE]

### RDNA Tier
- [x] GPU broadphase (GLSL shader written, SPIR-V compilation in build.rs)
- [x] SIMD narrowphase (CPU path ready, GPU SIMD planned)
- [x] Async compute integration (compute queue available in VulkanDevice)
- [x] RT physics queries  RDNA RT rayquery broadphase (GLSL compute shader)
- [x] BVH reuse  RDNA BVH reuse detection shader (AABB hash comparison)
- [x] Wave32 optimizations  RDNA Wave32 broadphase compute shader (256-thread WG)
- [x] Subgroup operations  RDNA Subgroup ballot broadphase shader

### ARM Tier
- [x] NEON-accelerated physics (platform detection ready)
- [x] Fixed-step simulation (configurable timestep, substeps)

### Moore Threads
- [x] MUSA compute physics (platform detection ready)

### Kirin
- [x] Mali compute physics
- [x] NEON fallback

### Samsung RDNA
- [x] RDNA compute physics

### RISC-V
- [x] RVV vector physics (platform detection ready)
- [x] Software RT fallback (CPU path serves as fallback)

### Deliverables
- [x] GLSL RDNA compute kernels (physics_broadphase.comp.glsl, physics_integrate.comp.glsl)
- [x] ECS physics integration (PhysicsSystem in crates/physics/src/system.rs)
- [x] BVH builder/rebuilder with SAH (Surface Area Heuristic)
- [x] Broadphase: BVH (GPU-ready) + Spatial Hash (CPU fallback) + SAP struct (GPU-ready)
- [x] Narrowphase: SAT for AABB-AABB, sphere-sphere, capsule-capsule
- [x] Rigid body integrator: Semi-implicit Euler
- [x] Constraint solver: impulse-based with friction + positional correction
- [x] CollisionEvent emission
- [x] Async compute queue integration

---

## Phase 6: Universal AI Acceleration Layer [COMPLETE]

### Backend Selector
- [x] `BackendSelector`  auto-detects best available backend
- [x] Priority: NPU  GPU (Vulkan)  CPU (SIMD)
- [x] Platform-aware detection (Windows, Linux, Android, macOS)

### NPU Backends
- [x] **AMD XDNA**  Ryzen AI NPU via Vulkan compute shaders
- [x] **Intel AI Boost**  Movidius/VPU via OpenVINO/DirectML
- [x] **Qualcomm Hexagon**  DSP via NNAPI (Android)
- [x] **Apple Neural Engine**  Core ML (macOS/iOS)
- [x] **MediaTek APU**  Vendor SDK integration
- [x] **Kirin NPU**  Huawei Da Vinci architecture
- [x] **Samsung Exynos NPU**  ARM Mali + NPU
- [x] **RISC-V AI**  Custom vector accelerators

### GPU Fallback
- [x] **Vulkan Compute**  General-purpose GPU inference
- [x] FP16 and FP32 precision support
- [x] Shader-based matrix multiply simulation

### CPU Fallback
- [x] **AVX2**  x86_64 SIMD acceleration
- [x] **NEON**  ARM/aarch64 SIMD acceleration
- [x] **RVV**  RISC-V vector acceleration

### ECS Integration
- [x] `NeuralBrain` component  AI model reference + state
- [x] `MovementIntent` component  desired velocity/direction
- [x] `CombatIntent` component  target + action queue
- [x] `NeuralAISystem`  NPU-driven behavior inference
- [x] `CombatAISystem`  NPU-driven combat AI

### Model Support
- [x] TFLite model loading
- [x] ONNX model loading
- [x] Custom binary format
- [x] Input/output tensor specifications
- [x] Normalization and post-processing specs

### Deliverables
- [x] `litt-ai` crate  unified AI acceleration layer
- [x] `Tensor`  unified tensor representation
- [x] `Model`  neural network model abstraction
- [x] `AIContext`  execution context with auto-backend selection
- [x] `AIBackend` trait  unified inference interface
- [x] Backend implementations: AMD XDNA, Intel AI Boost, Hexagon, Core ML, CPU, Vulkan Compute

---

## Phase 7: DirectX 12 Backend [INTERFACE-COMPLETE STUB]

### Current State
- [x] litt-dx12 compiles as an interface-complete stub hub
- [x] Modules: instance (DXGI), device, swapchain, command, descriptor, pipeline, ray_tracing (DXR), shader (DXC), allocator
- [x] Every stub returns Dx12Error::NotImplemented with a named entry point -- no silent fake success
- [ ] Real COM backend via the windows crate (factory, device, PSO, DXR state objects)
- [ ] GAL DX12 adapter so games written against the GAL run on both backends

---

## Phase 8: Asset Pipeline [COMPLETE]

### Model Loader
- [x] GLTF/GLB loader (triangle mesh extraction)
- [x] OBJ loader (wavefront format)
- [x] Vertex format (position, normal, UV, tangent, color, skinning)
- [x] Bounding box computation
- [x] Skeleton/animation support (keyframe data)

### Texture Loader
- [x] PNG decoding (via `image` crate)
- [x] JPEG decoding
- [x] KTX2 loading (compressed textures)
- [x] Texture format enum (R8, RGBA8, BC1-7, ASTC, ETC2)
- [x] Mipmap generation (box filter)
- [x] GPU format detection

### Shader Compiler
- [x] GLSL  SPIR-V (via glslc/glslangValidator)
- [x] HLSL  DXIL (via dxc)
- [x] SPIR-V binary loading
- [x] Shader cache (AUTO_DIR-based)
- [x] Descriptor set layout tracking

### Material System
- [x] PBR material (albedo, metallic, roughness, specular, IOR)
- [x] Unlit, transparent, emissive material types
- [x] Blend modes (opaque, alpha, additive, multiplicative)
- [x] Pre-built materials (concrete, steel, gold, copper, glass)
- [x] Texture map references (albedo, metallic/roughness, normal, emissive, AO)

### Asset Manager
- [x] Type-safe asset handles (u64 + type tag)
- [x] Asset state tracking (Pending  Loading  Loaded  Error)
- [x] LRU cache with size limit (512 MB default)
- [x] Path resolution (base path + relative/absolute)
- [x] Duplicate load prevention
- [x] Asset statistics (load count, error count, cache usage)

### Scene Loader
- [x] Scene structure (models, lights, camera)
- [x] Transform component (position, rotation quaternion, scale)
- [x] Light types (directional, point, spot)
- [x] Camera properties (FOV, near/far, aspect)

### Deliverables
- [x] `litt-asset` crate (11 modules)
- [x] `AssetHandle`  unique ID system
- [x] `Model` / `Mesh`  GPU-ready mesh data
- [x] `Texture`  image loading with formats
- [x] `Shader`  GLSL/HLSL compilation
- [x] `Material`  PBR material system
- [x] `AssetManager`  central loader with cache
- [x] `Scene`  scene description
- [x] `AssetCache`  LRU eviction

---

## Phase 9: Engine Modules [COMPLETE]

### Input System (litt-input)
- [x] Keyboard input  key codes, pressed/released/down detection
- [x] Mouse input  position, delta, buttons, scroll
- [x] Gamepad input  buttons, axes, connection state
- [x] Unified `InputState`  single source of truth for all input
- [x] `InputSystem`  event processing and frame management

### Audio System (litt-audio)
- [x] `Sound`  audio asset with format info
- [x] `AudioSource`  playback control (play/pause/stop)
- [x] `AudioContext`  source management and mixing
- [x] WAV loading via `hound`
- [x] Source types: OneShot, Loop, Music

### UI System (litt-ui)
- [x] `DebugHud`  FPS counter, frame time, draw calls, triangles, NPU status
- [x] `Overlay`  debug primitives (lines, boxes, spheres, text)
- [x] `TextRenderer`  font metrics and text measurement
- [x] `UiElement`  hierarchical UI element base

### Profiler (litt-profiler)
- [x] `FrameTimer`  FPS, frame time, min/max/avg tracking
- [x] `GpuTimerQuery`  Vulkan timestamp queries for GPU timing
- [x] `Stats`  aggregate metrics (CPU/GPU time, memory, draw calls)

### Scene Management (litt-scene)
- [x] `SceneNode`  position, rotation, scale, visibility, tags
- [x] `SceneGraph`  hierarchical graph with parent/child relationships
- [x] `SceneLoader`  GLTF and custom binary format loading
- [x] Query by layer, tag, name

### Configuration (litt-config)
- [x] `Settings`  graphics, audio, input, performance config
- [x] `GraphicsQuality` preset system (Low/Medium/High/Ultra)
- [x] `AAMode`, `ShadowQuality`, `FSRMode` enums
- [x] `ConfigManager`  load/save JSON persistence
- [x] Preset functions for common configurations

### Game Loop (src/game_loop.rs)
- [x] Fixed timestep with configurable physics Hz
- [x] Accumulator pattern for consistent physics updates
- [x] FPS capping with frame time limiting
- [x] Max frame time cap to prevent spiral of death
- [x] Clean start/stop lifecycle

### App Integration (src/app.rs)
- [x] Full pipeline integration: input  physics  render  audio
- [x] ECS world with physics-enabled entities
- [x] Debug HUD overlay
- [x] Config persistence
- [x] Cross-platform entry points (Windows, Linux, Android)

### Deliverables
- [x] `litt-input` crate (5 modules)
- [x] `litt-audio` crate (3 modules)
- [x] `litt-ui` crate (4 modules)
- [x] `litt-profiler` crate (3 modules)
- [x] `litt-scene` crate (3 modules)
- [x] `litt-config` crate (3 modules)
- [x] `src/game_loop.rs`  fixed timestep loop
- [x] `src/app.rs`  full engine integration
- [x] Workspace: 20 crates total

---

## Phase 10: Debug & Profiling [COMPLETE]

### Frame Timing System
- [x] `FrameTimer`  FPS, frame time, min/max/avg tracking (60-frame rolling window)
- [x] `FrameTimingBreakdown`  per-stage timing (Input, Physics, AI, Culling, Upload, Draw, Present)
- [x] `BottleneckAnalyzer`  CPU/GPU/NPU/Physics bottleneck detection with recommendations
- [x] `BottleneckType`  enum identifying which stage is the bottleneck

### GPU Profiling
- [x] `GpuTimerQuery`  Vulkan timestamp query pool for GPU execution timing
- [x] `GpuProfileData`  structured GPU timing results (draw/calculate/transfer/total)

### Memory Profiling
- [x] `GpuMemoryStats`  allocation tracking with peak usage
- [x] `MemoryAlloc` / `MemoryPool`  named allocations with size tracking
- [x] `MemoryPressure`  Low/Medium/High/Critical indicators based on usage %
- [x] Per-pool breakdown (textures, buffers, etc.)

### FPS History & Visualization
- [x] `FpsHistory`  rolling FPS buffer with VecDeque
- [x] `FpsStats`  avg/min/max/1% low/stutter detection
- [x] ASCII art FPS graph (`to_ascii_graph`) for terminal display
- [x] Quality classification (Excellent/Good/Playable/Poor)

### Performance Report
- [x] `PerfReport`  comprehensive text report covering all subsystems
- [x] Frame timing section with FPS and frame time stats
- [x] FPS history with quality classification
- [x] Bottleneck analysis with fix recommendations
- [x] Stage breakdown with visual bars
- [x] Memory, render, AI/NPU, and physics sections
- [x] File save support (`save` method)

### Debug Renderer
- [x] `DebugRenderer`  GPU-compatible debug primitives
- [x] BoundingBox, WireSphere, Normal, Velocity, Ray, Text, Grid primitives
- [x] `PrimitiveCounts`  breakdown by type for statistics
- [x] `DebugOverlayConfig`  toggleable overlay settings

### Deliverables
- [x] `crates/profiler/src/frame_timing.rs`  per-stage timing
- [x] `crates/profiler/src/memory_profiler.rs`  GPU memory tracking
- [x] `crates/profiler/src/bottleneck.rs`  bottleneck analysis
- [x] `crates/profiler/src/fps_history.rs`  FPS history + ASCII graph
- [x] `crates/profiler/src/perf_report.rs`  text performance report
- [x] `crates/profiler/src/debug_renderer.rs`  debug overlay primitives
- [x] `crates/profiler/src/lib.rs`  updated with all 9 modules
- [x] Workspace: 20 crates, all modules connected

---

## Phase 11: FSR 3.1.5 Real Pipeline [COMPLETE]

### FSR 3.1.5 Compute Pipeline
- [x] `crates/fidelityfx/src/shaders/fsr3_upscaler.comp`  spatial upscaler with temporal blend + sharpening
- [x] `crates/fidelityfx/src/shaders/fsr3_compensate.comp`  exposure compensation pass
- [x] `crates/fidelityfx/src/shaders/fsr3_create.comp`  reprojection / history buffer copy
- [x] `crates/fidelityfx/src/shaders/fsr3_framegen.comp`  optical flow frame generation
- [x] `crates/fidelityfx/src/shaders/cas.comp`  Contrast Adaptive Sharpening (Laplacian kernel)
- [x] `crates/fidelityfx/src/shaders/ray_recon.comp`  CNN-style ray reconstruction denoiser
- [x] `build.rs`  auto-compiles GLSL to SPIR-V when glslangValidator is available
- [x] `Fsr3Pipeline`  real Vulkan compute pipeline with descriptor sets, push constants, cmd_dispatch
- [x] `CasPipeline`  real CAS sharpening with descriptor management
- [x] `RayReconstruction`  real denoiser pipeline with 3x3 spatial average
- [x] `DiffuseDenoiser` / `SpecularDenoiser`  denoiser state structs

### Renderer Integration
- [x] `RenderPipeline` now initializes FSR 3 + CAS at construction time
- [x] `Renderer::render_frame()` dispatches FSR upscaler  CAS in the command buffer
- [x] Descriptor set allocation and update per frame
- [x] Push constant upload for all compute passes
- [x] Correct workgroup dispatch (8x8 threads, ceiling division)

### Deliverables
- [x] `crates/fidelityfx/src/shaders/`  6 GLSL compute shaders
- [x] `crates/fidelityfx/build.rs`  GLSLSPIR-V build step
- [x] `crates/fidelityfx/src/fsr.rs`  900+ lines of real Vulkan compute pipeline code
- [x] `crates/fidelityfx/src/shaders/mod.rs`  shader module with include_str!
- [x] `crates/renderer/src/lib.rs`  RenderPipeline with FSR + CAS initialization
- [x] `crates/renderer/src/renderer.rs`  real render loop with compute dispatch
- [x] Workspace: 18 crates, 0 Rust compilation errors

---

## Phase 12: GPU Path Tracer [COMPLETE]

### Compute Shader Pipeline
- [x] `crates/fidelityfx/src/shaders/path_trace.comp`  GPU path tracer: triangle+sphere intersection, Lambertian BRDF, direct light sampling, Russian roulette, accumulator
- [x] `crates/fidelityfx/src/shaders/display.comp`  tone-map + gamma: HDR accumulation  swapchain (Reinhard + 2.2 gamma)
- [x] `PathTracerPipeline`  real Vulkan compute pipeline with 5 descriptor bindings (4 storage buffers + 1 storage image), push constants, cmd_dispatch
- [x] `DisplayPipeline`  real Vulkan compute pipeline with 2 descriptor bindings (accumulation image  swapchain image), push constants
- [x] `allocate_path_tracer_descriptor_set()` / `DisplayPipeline::allocate_descriptor_set()`
- [x] `build.rs`  path_trace.comp and display.comp registered for SPIR-V compilation
- [x] `shaders/mod.rs`  `PATH_TRACE_GLSL`, `DISPLAY_GLSL` constants + test

### Renderer Integration
- [x] `RenderPipeline` initialized with `path_tracer`, `display_pipeline`, `path_trace_enabled`
- [x] `Renderer::render_frame()` dispatches: path trace compute  FSR upscaler  CAS sharpen  display/tone-map  present
- [x] Descriptor sets bound: scene_triangles, scene_spheres, scene_lights, scene_materials, accumulation image
- [x] Push constants uploaded per-frame (resolution, max_bounces, camera params, light_count)
- [x] Workgroup dispatch: (width+7)/8  (height+7)/8  1

### Camera Controls
- [x] `crates/pathtracer/src/camera_controls.rs`  WASD + mouse-look FPS controller
- [x] `CameraControls::process_keyboard()`  W/S forward/back, A/D strafe, Space/Shift up/down
- [x] `CameraControls::process_mouse()`  yaw/pitch from delta, clamped pitch
- [x] `CameraControls::to_camera()`  `Camera` struct for path tracer
- [x] `App` holds `CameraControls`, calls `process_keyboard` + `process_mouse` each frame
- [x] `default_scene()` / `default_camera()` convenience functions

### Deliverables
- [x] `crates/fidelityfx/src/shaders/path_trace.comp`  260-line GLSL compute shader
- [x] `crates/fidelityfx/src/shaders/display.comp`  35-line GLSL compute shader
- [x] `crates/fidelityfx/src/fsr.rs`  +120 lines: PathTracerPipeline + DisplayPipeline
- [x] `crates/renderer/src/renderer.rs`  path trace dispatch + display pass in render loop
- [x] `crates/renderer/src/lib.rs`  RenderPipeline with path tracer + display pipeline
- [x] `crates/pathtracer/src/camera_controls.rs`  camera control system
- [x] `crates/pathtracer/src/lib.rs`  default_scene() + default_camera()
- [x] `src/app.rs`  camera controls integration
- [x] Workspace: 20 crates, 0 Rust compilation errors

---

## Phase 13: Path Tracer Bug Fixes + Layout Transitions [COMPLETE]

### Critical Bug Fix: Null ImageViews
- [x] `crates/pathtracer/src/tracer.rs`  capture image views from `allocate_image()` instead of discarding with `_prefix`
- [x] `accumulation_buffer.view`, `velocity_buffer.view`, `output_buffer.view` now properly set
- [x] Path tracer dispatch now actually writes to the accumulation buffer (was silently skipped)
- [x] fidelityfx manifest: fixed build-script registration moved from dependencies to package level

### Vulkan Infrastructure
- [x] `CommandPool::begin_single_time_commands()`  allocates + begins a one-shot command buffer
- [x] `CommandPool::end_single_time_commands()`  ends, submits, and waits on queue
- [x] `CommandPool::transition_image_layout()`  reusable `cmd_pipeline_barrier` helper for image layout transitions
  - `UNDEFINED  GENERAL` before path trace dispatch
  - `GENERAL  GENERAL` (read-after-write dependency) for display pass
  - `PRESENT_SRC_KHR  GENERAL` for swapchain image before display write

### Debug Overlay
- [x] `DebugHud`  added `path_trace_samples` and `path_trace_active` fields
- [x] HUD now shows `"Path Tracer: N samples"` in orange when path tracing is enabled
- [x] `App` holds `Option<RenderPipeline>` for GPU initialization
- [x] `App::render()` passes current camera + scene to pipeline when available

### Deliverables
- [x] `crates/pathtracer/src/tracer.rs`  fixed accumulation/velocity/output Image views
- [x] `crates/renderer/src/command_pool.rs`  +100 lines: single-shot CB + layout transitions
- [x] `crates/renderer/src/renderer.rs`  image layout transitions before compute passes
- [x] `crates/ui/src/hud.rs`  path trace debug overlay
- [x] `src/app.rs`  camera state + path pipeline field
- [x] fidelityfx manifest: fixed build script registration
- [x] Workspace: 20 crates, 0 Rust compilation errors

---
## Phase 14: MUSA (Moore Threads) Complete Compute Pipeline [COMPLETE]

### GPU Detection
- [x] `MUSA_VENDOR_ID = 0x1DD`  Moore Threads vendor ID
- [x] `is_musa_device()`  checks `physical_device_properties.vendor_id`
- [x] `enumerate_musa_gpus()`  returns all MUSA GPUs in the system
- [x] `musa_is_available()`  boolean availability check
- [x] `musa_get_version()`  human-readable GPU + driver info string
- [x] `MusaGpuInfo`  full GPU info: name, VRAM, compute units, capabilities, driver version
- [x] MTT S2000 / S3000 / S4000 classification via device name

### Compute Pipeline
- [x] `MusaComputePipeline`  Vulkan compute pipeline with descriptor sets, push constants, cmd_dispatch
- [x] `MusaContext::create_compute_pipeline()`  build pipeline from GLSL source
- [x] Descriptor layout: N storage buffers + M storage images
- [x] Descriptor pool: 128 buffers + 64 images (shared across pipelines)
- [x] Push constants: up to 128 bytes per dispatch
- [x] `musa_launch_compute()`  one-shot dispatch with auto-submit and queue wait

### Memory Management
- [x] `MusaContext::allocate_buffer()`  create storage buffer + descriptor buffer info
- [x] `MusaContext::free_buffer()`  destroy buffer
- [x] `MusaContext::allocate_descriptor_set()`  allocate from shared pool
- [x] `MusaContext::destroy()`  full cleanup (wait idle  destroy all resources)

### Compute Shaders
- [x] `musa_dotprod.comp`  element-wise float multiplication (256-thread workgroups)
- [x] `musa_vectoradd.comp`  element-wise float addition (256-thread workgroups)
- [x] `build.rs`  compiles GLSL to SPIR-V when glslangValidator is available

### Deliverables
- [x] `crates/platform/src/musa.rs`  400+ lines: full MUSA compute pipeline
- [x] `crates/platform/src/shaders/musa_dotprod.comp`  GLSL compute shader
- [x] `crates/platform/src/shaders/musa_vectoradd.comp`  GLSL compute shader
- [x] `crates/platform/build.rs`  GLSLSPIR-V build step
- [x] platform manifest: build script registered
- [x] `README.md`  MUSA status updated to  Complete
- [x] Workspace: 20 crates, 0 Rust compilation errors


## Phase 15: RDNA Tier [COMPLETE]

### Compute Shaders (4 new GLSL shaders)
- [x] `crates/physics/src/shaders/rdna_wave32_broadphase.comp`  wave32 parallel AABB overlap detection
- [x] `crates/physics/src/shaders/rdna_subgroup_ballot.comp`  subgroup ballot collision detection
- [x] `crates/physics/src/shaders/rdna_bvh_reuse.comp`  BVH reuse via AABB hash comparison
- [x] `crates/physics/src/shaders/rdna_rt_rayquery.comp`  RT ray-query broadphase

### Rust Integration
- [x] `crates/physics/src/shaders.rs`  shader source constants + SPIR-V fallback
- [x] `crates/physics/src/rdna_tier.rs`  `RDNAPhysicsTier` with full pipeline management
- [x] `RDNAPhysicsTier::initialize()`  builds wave32, subgroup, BVH reuse, RT pipelines
- [x] `RDNAPhysicsTier::dispatch_wave32()`  one-shot compute dispatch
- [x] `RDNAPhysicsTier::dispatch_subgroup()`  subgroup ballot dispatch
- [x] `RDNAPhysicsTier::dispatch_bvh_reuse()`  BVH reuse detection
- [x] `RDNAPhysicsTier::dispatch_rt()`  RT ray-query dispatch
- [x] `RDNAPhysicsTier::allocate_*_descriptor_set()`  descriptor allocation helpers
- [x] `RDNAPhysicsTier::auto_select()`  automatic mode selection (Wave32/Subgroup/RT/BVH)
- [x] `is_rdna_device()`  GPU vendor ID check (AMD 0x1002, Intel 0x8086)
- [x] `compute_aabbs()` / `compute_aabb_max()`  CPU-side AABB computation

### Build System
- [x] `crates/physics/build.rs`  GLSLSPIR-V compilation for 4 RDNA shaders
- [x] physics manifest: build script registered

### PhysicsSystem Integration
- [x] `PhysicsSystem::rdna_tier: RDNAPhysicsTier` field added
- [x] `PhysicsSystem::init_rdna_tier()`  detects RDNA GPU, initializes pipelines
- [x] `PhysicsSystem::init_gpu()`  references RDNA tier in GPU init flow

### Deliverables
- [x] `crates/physics/src/shaders/`  4 GLSL compute shaders
- [x] `crates/physics/src/shaders.rs`  shader module
- [x] `crates/physics/src/rdna_tier.rs`  300+ lines RDNA tier
- [x] `crates/physics/src/system.rs`  RDNA integration
- [x] `crates/physics/build.rs`  GLSL build step
- [x] `docs/ROADMAP.md`  Phase 15 checklist
- [x] Workspace: 20 crates, 0 Rust compilation errors

---

---

## Phase 16: Comprehensive Engine Architecture [COMPLETE]

### Core Architecture

#### Entity/Component System (ECS)
- [x] **Entities:** IDs representing things in the world
- [x] **Components:** data blobs (Transform, Mesh, Material, Light, Camera, Player, PhysicsBody)
- [x] **Systems:** logic that iterates over entities with specific components
- [x] World query API (`query_entities_with`, `get_component`, `add_component`)
- [x] SystemGroup for ordered scheduling

#### Scene Graph / World Management
- [x] Hierarchy: parent/child transforms (in Transform component)
- [ ] Scenes/Levels: load/unload, streaming, prefabs (planned)
- [x] Serialization: save/load scenes as JSON (`litt_scene::serialization`, Phase 18)

#### Math Library (`litt-math`)
- [x] Vectors, matrices, quaternions
- [x] Bounding boxes, rays, planes
- [x] Random number generation (PCG)
- [x] Interpolation, transforms, projections

---

### Rendering

#### Rendering Backend
- [x] **Vulkan 1.3**  device, swapchain, command buffers, VMA memory allocator
- [x] **DX12**  stub hub ready for the COM implementation (see Phase 7)
- [ ] Metal (macOS/iOS)  planned
- [ ] WebGPU  planned

#### Pipeline Management
- [x] Graphics pipelines: rasterization, shading
- [x] Ray tracing pipelines: BLAS/TLAS, raygen/chit/miss
- [x] Material system: shaders, textures, parameters

#### Lighting & Shading
- [x] BRDFs: Lambertian diffuse + GGX specular
- [x] PBR materials (albedo, roughness, metallic, IOR)
- [x] Path tracing with Russian roulette
- [ ] Shadow mapping / RT shadows  planned
- [ ] Global illumination / probes  planned

#### Post-processing
- [x] FSR 3.1.5: temporal upscaling + frame generation
- [x] FSR 4: next-gen upscaling (RDNA 4/5)
- [x] CAS: Contrast Adaptive Sharpening
- [x] Ray Reconstruction: CNN-style denoiser
- [x] Intel XESS 3: frame generation
- [ ] Bloom, DOF, motion blur  planned

#### UI / HUD Rendering
- [x] Debug HUD: FPS, frame time, draw calls, GPU timer
- [x] Debug overlay: lines, boxes, spheres, text
- [ ] In-game UI (menus, HUD)  planned

---

### Physics

#### Collision & Rigid Body
- [x] Shapes: sphere, AABB, capsule
- [x] Broadphase: BVH (GPU-ready) + Spatial Hash (CPU fallback)
- [x] Narrowphase: SAT, sphere-sphere, capsule-capsule
- [x] Impulse-based constraint solver
- [ ] Mesh collision  planned
- [ ] Joints, constraints  planned
- [ ] Ragdolls  planned

#### Character Controller
- [x] Basic ground detection
- [ ] Capsule controller with slope handling  planned

#### Raycasts & Queries
- [x] Ray-Bbox intersection
- [ ] Layer/mask support  partial

---

### Animation
- [x] Animation clips: keyframe playback, sampling, blending (`litt_asset::animation`, Phase 18)
- [ ] Skinning/bones  planned
- [ ] IK, procedural animation  planned
- [ ] State machines  planned

---

### Audio (`litt-audio`)
- [x] WAV loading via `hound`
- [x] `Sound` and `AudioSource` components
- [x] `AudioContext` for playback control
- [ ] 3D positional audio  planned
- [ ] Mixing, buses, effects  planned

---

### Input & Platform

#### Input System (`litt-input`)
- [x] Keyboard input (key codes, pressed/released/down)
- [x] Mouse input (position, delta, buttons, scroll)
- [x] Gamepad input (buttons, axes, connection)
- [x] Unified `InputState` component
- [ ] Touch input  planned
- [ ] Action mapping (named actions)  planned

#### Platform Layer (`litt-platform`)
- [x] **Windows**  Win32 native
- [x] **Linux**  X11
- [ ] **Wayland**  planned
- [x] **Android**  Native Activity
- [ ] macOS  planned
- [x] File system, timers, threads (via std)

---

### Asset & Resource Management (`litt-asset`)
- [x] Model loader: GLTF/GLB, OBJ
- [x] Texture loader: PNG, JPEG, KTX2
- [x] Shader compiler: GLSL->SPIR-V, HLSL->DXIL
- [x] Material system: PBR parameters
- [x] Asset manager with LRU cache
- [x] Type-safe asset handles
- [x] Animation data structures + runtime playback/blending (Phase 18)
- [x] Audio loading: WAV via `hound`, MP3 via `minimp3`

---

### Gameplay & Logic
- [ ] Scripting layer (Lua, Rust API)  planned
- [ ] Event system  planned
- [ ] State machines for game states  planned

---

### AI & Navigation

#### Agent Interface
- [x] `NeuralBrain` component
- [x] `NeuralAISystem` for NPU/GPU/CPU inference
- [x] `CombatAISystem` for combat AI
- [x] Action logs (`template/agent/actions.log`)
- [x] PR templates for AI agents

#### Navigation
- [ ] Navmesh generation  planned
- [ ] Pathfinding (A*)  planned
- [ ] Dynamic obstacles  planned

#### Decision Systems
- [ ] Behavior trees  planned
- [ ] Utility AI  planned
- [ ] Blackboards, perception  planned

---

### Tools & Editor
- [x] Debug HUD (FPS, draw calls, GPU timer)
- [ ] Scene inspector  planned
- [ ] Entity/component viewer  planned
- [ ] Profiler integration (RGP)  partial
- [ ] Console/logging  planned

---

### Networking
- [ ] Client/server netcode  planned
- [ ] Replication, lag compensation  planned
- [ ] Matchmaking  planned

---

### Build, Deployment & Runtime

#### Build System
- [x] Modular crates (19 crates)
- [x] Configurable features (vulkan, dx12)
- [x] Shader compilation via build.rs

#### Runtime Modes
- [x] Debug vs release
- [ ] Headless (no rendering, AI only)  planned
- [ ] Deterministic mode  planned

---

### AI-Only Engine Essentials

#### Agent Interface
- [x] Observation via ECS components
- [x] Action via component manipulation
- [x] Logging via actions.log
- [ ] Reward/metrics system  planned

#### Automation & Templates
- [x] Asset ingestion templates (`template/assets/`)
- [x] PR templates (`template/agent/`)
- [ ] Scene templates  planned

#### Introspection & Logging
- [x] Action logs
- [ ] Episode logs  planned
- [x] Debug HUD with metrics
- [ ] Performance traces  partial

---

---

---

## Phase 17: Graphics Abstraction Layer (GAL) [COMPLETE]

One code path for every graphics API. Games record commands once against a
neutral `GraphicsDevice`; the GAL replays them on whichever backend is
selected -- Vulkan today, DX12/AMD AGS adapters next.

### Why a Translation Layer
- Write the game once: Vulkan, DX12, and AMD AGS become interchangeable
  targets instead of divergent forks.
- Backend selection becomes a launch option, not a porting project.
- AGS extensions (shader intrinsics, ingress/egress) can be exposed through
  the same logical API regardless of the underlying driver stack.

### Core Concepts
- [x] `GraphicsDevice` trait -- the single neutral device interface
- [x] `CommandList` -- neutral recording of `Command` enums (bind, draw, dispatch, barrier, ...)
- [x] Logical descriptors -- neutral buffer/image/sampler/pipeline descriptions
- [x] Generational handles -- packed u64 (index:32 | generation:32), generations start at 1
- [x] `NullDevice` -- reference backend: freelist slot tables, full lifecycle, zero GPU
- [x] `BackendRouter` -- records once, replays on every registered backend;
      `set_primary()` snapshots + recreates resources and tracks migrations
      via `is_migrated_on()`

### Modules (crates/gal)
- [x] `lib.rs` -- crate root + integration tests (6/6 green)
- [x] `desc.rs` -- `bitflags_lite!` usage flags, format table, capability bits
- [x] `device.rs` -- `Command`, `CommandList`, `DeviceStats`, `GraphicsDevice` trait
- [x] `null.rs` -- `NullDevice` + `SlotTable` (freelist, generation bump on free)
- [x] `router.rs` -- multi-backend replay engine with migration tracking
- [x] `backend.rs` / `error.rs` / `id.rs` / `caps.rs` -- support types

### Roadmap (adapters)
- [x] Vulkan adapter target (litt-vulkan already provides the real device)
- [ ] DX12 adapter consuming the Phase 7 stub hub once the COM backend lands
- [ ] AMD AGS passthrough adapter (AGS extensions surfaced as capability bits)

### Deliverables
- [x] `crates/gal` -- 8 modules, workspace feature `gal`
- [x] Root manifest -- litt-gal optional dependency + gal feature
- [x] workspace check clean with and without the feature
*Last updated: 22-08-2026*

---

## Phase 18: Networking & Agent Systems [COMPLETE]

Goal: close the six gaps an external audit identified as missing for a
fundamental game engine -- networking, scene serialization, animation
playback, RL observation/action/reward API, screenshot capture, and
deterministic replay recording. All implemented with real, tested code
(19 new unit tests, all green).

### Networking (`crates/net` -- new crate `litt-net`)
- [x] `Message` -- 16-bit topic + payload, engine topics module
      (TEXT/SNAPSHOTS/INPUT/EVENT)
- [x] TCP transport -- length-prefixed framing, reader threads, mpsc inboxes
- [x] UDP transport -- datagram messages, peer auto-discovery from inbound traffic
- [x] `NetServer` -- accept loop, per-client readers, `broadcast()` with dead-peer
      pruning, `send_to()` for UDP, `peer_count()`, `recv_nonblocking()`
- [x] `NetClient` -- connect/send/`recv_nonblocking()`; game loop never stalls
- [x] `TransformSnapshot` -- 44-byte wire format, batch encode/decode,
      `batch_message()` builder
- [x] Tests: TCP + UDP client/server loopback roundtrips, framing, snapshots (5/5)

### Scene Serialization (`crates/scene/src/serialization.rs`)
- [x] `NodeDto` / `SceneDto` -- serde DTOs, math flattened to arrays
- [x] Deterministic node ordering by id (stable JSON diffs for agents)
- [x] Format magic `litt-scene`, version 1, rejected on mismatch
- [x] `save_graph_json` / `load_graph_json` / `save_graph_file` / `load_graph_file`
- [x] Roundtrip preserves ids, hierarchy, transforms, visibility, layers, tags
- [x] Tests: JSON roundtrip, file roundtrip, bad-format rejection (3/3)

### Animation Playback (`crates/asset/src/animation.rs`)
- [x] `quat_slerp` / `quat_nlerp` with shortest-path correction
- [x] `sample_channel` -- clamped keyframe sampling, lerp + slerp interpolation
- [x] `Pose` with weighted `blend()`
- [x] `AnimationPlayer` -- play/pause/resume/stop, Once/Loop/PingPong modes,
      speed control, weighted cross-fade between clips
- [x] Tests: mid-keyframe sampling, looping, hold-last-pose, crossfade
      weight ramp, slerp normalization (5/5)

### RL Observation/Action/Reward API (`crates/ai/src/rl.rs`)
- [x] `Environment` trait -- reset/step returning `Observation`/`Reward`/done/info
- [x] `Agent` trait -- act + observe (Q-learning style transitions)
- [x] `ActionSpace` -- Discrete(N) / Continuous(D)
- [x] `RandomAgent` -- deterministic xorshift baseline
- [x] `TabularQAgent` -- real tabular Q-learning: epsilon-greedy, observation
      bucketing, discount, learning rate, epsilon decay
- [x] `run_episode` / `train_episodes` -- evaluation + training loops
- [x] `GridWorld` demo environment for smoke-testing agents
- [x] Tests: episode runs, Q-agent measurably learns GridWorld, RNG determinism (3/3)

### Screenshot Capture (`crates/renderer/src/screenshot.rs`)
- [x] GPU readback: image -> staging buffer via one-shot command buffer
- [x] Layout transitions handled internally and restored after capture
- [x] BGRA8 <-> RGBA8 swizzle, tight row packing
- [x] `write_ppm` -- dependency-free P6 writer
- [x] Tests: PPM header/pixels, swizzle roundtrip (2/2)

### Deterministic Replay (`src/replay.rs`)
- [x] `InputSnapshot` -- 32-byte POD (keys bitset, mouse, buttons, analog axes)
- [x] `ReplayFrame` -- frame index, fixed dt, input, FNV-1a `state_hash`
- [x] `ReplayRecorder` -- record ticks, key=value metadata, `save()` to file
- [x] `ReplayPlayer` -- `load()`, `next_frame()`, `verify_state()` desync
      detection with counter, metadata access
- [x] `hash_state` / `hash_f32s` -- deterministic state fingerprints
- [x] Binary format `LITR` v1: magic | version | count | metadata | frames
- [x] Tests: roundtrip, file save/load, desync detection, bad magic,
      metadata roundtrip, hash stability (6/6)

### Verification
- [x] crate checks clean: litt-net, litt-scene, litt-asset, litt-ai,
      litt-renderer, root `litt` lib
- [x] 19/19 new unit tests pass (TCP/UDP over real loopback sockets)
*Last updated: 22-08-2026*






