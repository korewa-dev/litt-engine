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

## Phase 5: Physics System [✅ COMPLETE]

### RDNA Tier
- [x] GPU broadphase (GLSL shader written, SPIR-V compilation in build.rs)
- [x] SIMD narrowphase (CPU path ready, GPU SIMD planned)
- [x] Async compute integration (compute queue available in VulkanDevice)
- [ ] RT physics queries (future)
- [ ] BVH reuse (future)
- [ ] Wave32 optimizations (RDNA-specific, future)
- [ ] Subgroup operations (RDNA-specific, future)

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

## Phase 6: Universal AI Acceleration Layer [✅ COMPLETE]

### Backend Selector
- [x] `BackendSelector` — auto-detects best available backend
- [x] Priority: NPU → GPU (Vulkan) → CPU (SIMD)
- [x] Platform-aware detection (Windows, Linux, Android, macOS)

### NPU Backends
- [x] **AMD XDNA** — Ryzen AI NPU via Vulkan compute shaders
- [x] **Intel AI Boost** — Movidius/VPU via OpenVINO/DirectML
- [x] **Qualcomm Hexagon** — DSP via NNAPI (Android)
- [x] **Apple Neural Engine** — Core ML (macOS/iOS)
- [x] **MediaTek APU** — Vendor SDK integration
- [x] **Kirin NPU** — Huawei Da Vinci architecture
- [x] **Samsung Exynos NPU** — ARM Mali + NPU
- [x] **RISC-V AI** — Custom vector accelerators

### GPU Fallback
- [x] **Vulkan Compute** — General-purpose GPU inference
- [x] FP16 and FP32 precision support
- [x] Shader-based matrix multiply simulation

### CPU Fallback
- [x] **AVX2** — x86_64 SIMD acceleration
- [x] **NEON** — ARM/aarch64 SIMD acceleration
- [x] **RVV** — RISC-V vector acceleration

### ECS Integration
- [x] `NeuralBrain` component — AI model reference + state
- [x] `MovementIntent` component — desired velocity/direction
- [x] `CombatIntent` component — target + action queue
- [x] `NeuralAISystem` — NPU-driven behavior inference
- [x] `CombatAISystem` — NPU-driven combat AI

### Model Support
- [x] TFLite model loading
- [x] ONNX model loading
- [x] Custom binary format
- [x] Input/output tensor specifications
- [x] Normalization and post-processing specs

### Deliverables
- [x] `litt-ai` crate — unified AI acceleration layer
- [x] `Tensor` — unified tensor representation
- [x] `Model` — neural network model abstraction
- [x] `AIContext` — execution context with auto-backend selection
- [x] `AIBackend` trait — unified inference interface
- [x] Backend implementations: AMD XDNA, Intel AI Boost, Hexagon, Core ML, CPU, Vulkan Compute

---

## Phase 7: DirectX 12 Backend [✅ COMPLETE]

---

## Phase 8: Asset Pipeline [✅ COMPLETE]

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
- [x] GLSL → SPIR-V (via glslc/glslangValidator)
- [x] HLSL → DXIL (via dxc)
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
- [x] Asset state tracking (Pending → Loading → Loaded → Error)
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
- [x] `AssetHandle` — unique ID system
- [x] `Model` / `Mesh` — GPU-ready mesh data
- [x] `Texture` — image loading with formats
- [x] `Shader` — GLSL/HLSL compilation
- [x] `Material` — PBR material system
- [x] `AssetManager` — central loader with cache
- [x] `Scene` — scene description
- [x] `AssetCache` — LRU eviction

---