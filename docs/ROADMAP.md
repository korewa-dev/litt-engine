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

## Phase 6: Universal AI Acceleration Layer [PLANNED]
