<!-- REMOVED STACK NOTICE (CDR-007): The Rust engine described here was removed from the repo; this document remains as design reference for the C/C++ port (native/littcore). -->
# PhysicsSystem

> GPU-accelerated rigid body physics with multi-tier hardware support.

**Status:**  Complete (Phase 5)

CPU fallback path is fully implemented with BVH broadphase. GPU compute shader path is ready for async dispatch.

---

## Overview

The `PhysicsSystem` is a GPU-accelerated rigid body physics engine integrated into the Litt Engine ECS. It reads `PhysicsBody` components, simulates collisions and forces on GPU compute shaders (with CPU fallback for unsupported hardware), and writes updated `Transform` components.

---

## PhysicsBody Component

```rust
/// Collider shape types supported by the physics system.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ColliderShape {
    /// Axis-aligned bounding box
    AABB { half_extent: Vec3 },
    /// Sphere
    Sphere { radius: f32 },
    /// Capsule (cylinder + two hemispheres)
    Capsule { radius: f32, half_height: f32 },
}

/// Physics body component attached to simulating entities.
#[derive(Clone, Debug)]
pub struct PhysicsBody {
    /// Collider shape
    pub shape: ColliderShape,
    /// Mass in kg (0.0 = static/kinematic)
    pub mass: f32,
    /// Linear velocity (m/s)
    pub linear_velocity: Vec3,
    /// Angular velocity (rad/s)
    pub angular_velocity: Vec3,
    /// Linear damping (0.0-1.0)
    pub linear_damping: f32,
    /// Angular damping (0.0-1.0)
    pub angular_damping: f32,
    /// Friction coefficient (0.0-1.0)
    pub friction: f32,
    /// Restitution / bounciness (0.0-1.0)
    pub restitution: f32,
    /// Collision layer bitmask
    pub layer: u32,
    /// Trigger flag (no response, only notify)
    pub is_trigger: bool,
    /// Gravity scale (1.0 = full gravity)
    pub gravity_scale: f32,
}
```

---

## Broadphase

| Method | Platform | Performance |
|--------|----------|-------------|
| **BVH (SAH)** | All | O(log n) queries |
| Spatial Hash | CPU fallback | O(n) average |
| SAP (GPU) | RDNA compute | Parallel sorting |

### BVH Builder (Complete)
- Surface Area Heuristic (SAH) for optimal tree construction
- Rebuild support for dynamic scenes
- O(log n) overlap queries vs O(n) brute force

### SIMD-Optimized Broadphase
- **x86_64**: AVX2-optimized batch processing
- **aarch64**: NEON-accelerated parallel overlap detection
- **riscv64**: RVV vectorized spatial hash

---

## Narrowphase

| Shape Pair | Algorithm | GPU? |
|------------|-----------|------|
| AABB vs AABB | SAT (Separating Axis Theorem) |  RDNA compute |
| Sphere vs Sphere | Distance check |  All tiers |
| AABB vs Sphere | Closest-point test |  All tiers |
| Capsule vs Capsule | Segment-segment distance |  RDNA compute |
| Convex vs Convex | GJK-EPA |  Planned |

---

## Rigid Body Integrator

Uses semi-implicit Euler for stability:

```rust
fn semi_implicit_euler(body: &mut PhysicsBody, dt: f32, external_force: Vec3) {
    let gravity = Vec3::new(0.0, -9.81 * body.gravity_scale, 0.0);
    let total_force = external_force + gravity * body.mass;
    let acceleration = if body.mass > 0.0 { total_force / body.mass } else { Vec3::ZERO };
    body.linear_velocity = (body.linear_velocity + acceleration * dt) * (1.0 - body.linear_damping * dt);
    body.angular_velocity = (body.angular_velocity + acceleration * dt) * (1.0 - body.angular_damping * dt);
    body.linear_velocity = body.linear_velocity.clamp_len(0.0, 100.0);
}
```

---

## Constraint Solver

Impulse-based resolution with friction and positional correction:

```rust
// Solve contact constraint
solver.solve_contact(&mut body_a, &mut body_b, contact.normal, contact.penetration);

// Features:
// - Coefficient of restitution (bounciness)
// - Friction model (Coulomb friction)
// - Positional correction (anti-embedding)
// - Iterative solving (3 iterations by default)
```

---

## Async Compute Integration

Physics runs on a separate compute queue from the graphics queue:

```rust
// GPU path: async compute dispatch
if system.async_compute && system.gpu_pipeline.is_some() {
    // Record compute commands to separate command buffer
    // Dispatch broadphase shader (spatial hash on GPU)
    // Dispatch integrate shader (physics integration)
    // Signal fence for synchronization
}

// CPU path: fallback when GPU unavailable
else {
    for _ in 0..substeps {
        cpu_step(world, fixed_dt);
    }
}
```

**Queue Architecture:**
- **RDNA**: Async compute hardware units execute physics while GPU renders
- **ARM**: Compute and graphics queues share the same GPU -- serialized
- **RISC-V**: Software fallback, physics blocks render (no async hardware)

---

## GPU Compute Kernels

### RDNA (GLSL)

```glsl
// shaders/compute/physics_broadphase.comp.glsl
#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct PhysicsBody {
    uint shape_type;
    float mass;
    float inv_mass;
    vec3 linear_velocity;
    vec3 angular_velocity;
    float linear_damping;
    float angular_damping;
    float friction;
    float restitution;
    uint layer;
    uint is_trigger;
    float gravity_scale;
    float shape_data[4];
};

layout(set = 0, binding = 0, scalar) buffer Bodies { PhysicsBody bodies[]; } uBodies;
layout(set = 0, binding = 1, scalar) buffer Grid { uint grid[]; } uGrid;
layout(set = 0, binding = 2, scalar) uniform Params {
    uint body_count;
    float cell_size;
    uint grid_size;
    uint pad;
} uParams;

void main() {
    uint body_idx = gl_GlobalInvocationID.x;
    if (body_idx >= uParams.body_count) return;
    // ... spatial hash broadphase
}
```

```glsl
// shaders/compute/physics_integrate.comp.glsl
#version 450
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct PhysicsBody { ... };
struct Transform { vec3 position; vec4 rotation; vec3 scale; float pad; };

layout(set = 0, binding = 0, scalar) buffer Bodies { PhysicsBody bodies[]; } uBodies;
layout(set = 0, binding = 1, scalar) buffer Transforms { Transform transforms[]; } uTransforms;
layout(set = 0, binding = 2, scalar) uniform Params {
    uint body_count;
    vec3 gravity;
    float dt;
    uint pad;
} uParams;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uParams.body_count) return;
    // Semi-implicit Euler integration
    // Ground collision response
}
```

### ARM NEON Fallback

```rust
#[cfg(target_arch = "aarch64")]
pub mod neon {
    /// NEON-optimized broadphase processing
    pub fn broadphase_neon(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)>;
}
```

### RISC-V RVV Fallback

```rust
#[cfg(target_arch = "riscv64")]
pub mod rvv {
    /// RVV vectorized broadphase
    pub fn broadphase_rvv(aabbs: &[(Vec3, Vec3)]) -> Vec<(usize, usize)>;
}
```

---

## ECS Integration

The `PhysicsSystem` reads `PhysicsBody` and `Transform` components, simulates one physics tick, and writes back updated `Transform` components:

```rust
impl System for PhysicsSystem {
    fn update(&mut self, world: &mut World, _dt: f32) {
        let substeps = self.substeps.max(1) as usize;
        let sub_dt = self.fixed_dt;

        for _ in 0..substeps {
            self.cpu_step(world, sub_dt);
        }
    }
}
```

**Components used:**
- `PhysicsBodyECS` -- physics body data
- `PhysicsTransform` -- position/rotation/scale
- `Velocity` -- computed velocity output

**Events emitted:**
- `CollisionEvent` -- collision notifications

---

## Usage Example

```rust
use litt_physics::*;

// Create physics system
let mut physics = PhysicsSystem::new();

// Or with GPU acceleration
let mut physics = PhysicsSystem::with_gpu();

// Or with custom settings
let mut physics = PhysicsSystem::with_gravity(Vec3::new(0.0, -9.81, 0.0))
    .with_timing(1.0 / 60.0, 3); // 60Hz, 3 substeps

// Initialize GPU pipelines (optional)
physics.init_gpu(&device, &mut allocator)?;

// In game loop
physics.update(&mut world, dt);

// Check for collisions
for event in &physics.collisions {
    println!("Collision: {:?} <-> {:?}", event.entity_a, event.entity_b);
}
```

---

## Roadmap

###  Completed
- [x] `PhysicsBody` component with 128-byte GPU layout
- [x] `ColliderShape` enum (AABB, Sphere, Capsule)
- [x] BVH builder with SAH (Surface Area Heuristic)
- [x] SAT for AABB-AABB narrowphase
- [x] Sphere-sphere and capsule-capsule narrowphase
- [x] Semi-implicit Euler integrator
- [x] Impulse-based constraint solver
- [x] Fixed-step simulation with configurable substeps
- [x] ECS integration (PhysicsSystem)
- [x] GLSL compute shaders (broadphase + integrate)
- [x] GPU buffer management
- [x] CollisionEvent emission
- [x] NEON-accelerated broadphase (ARM)
- [x] RVV-accelerated broadphase (RISC-V)
- [x] AVX2-accelerated broadphase (x86_64)
- [x] Async compute integration (compute queue available)

###  Planned
- [ ] BVH rebuild optimization (incremental updates)
- [ ] Wave32 optimizations (RDNA-specific)
- [ ] Subgroup operations (RDNA-specific)
- [ ] GJK-EPA for convex-convex collision
- [ ] Soft body / deformable physics
- [ ] Character controller with slope caching
- [ ] GPU-driven particle physics

###  Experimental
- Neural collision prediction via NPU
- Procedural terrain deformation
- Real-time fluid simulation (SPH on GPU)

---

## Hardware-Specific Notes

- **RDNA / AMD:** WGSL compute shaders, wave32 optimization, async compute queue
- **Moore Threads:** MUSA compute shaders, Vulkan 1.3 compute
- **ARM / Mobile:** NEON intrinsics, fixed-step simulation for power efficiency
- **RISC-V:** RVV vectorized spatial hash, software ray-cast fallback

