﻿# PhysicsSystem

> GPU-accelerated rigid body physics with multi-tier hardware support.

**Status:** 📋 Planned — Phase 5 of [ROADMAP.md](./ROADMAP.md). No implementation exists yet.

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

impl Default for PhysicsBody {
    fn default() -> Self {
        Self {
            shape: ColliderShape::AABB { half_extent: Vec3::new(0.5, 0.5, 0.5) },
            mass: 1.0,
            linear_velocity: Vec3::ZERO,
            angular_velocity: Vec3::ZERO,
            linear_damping: 0.1,
            angular_damping: 0.1,
            friction: 0.6,
            restitution: 0.2,
            layer: 0xFFFFFFFF,
            is_trigger: false,
            gravity_scale: 1.0,
        }
    }
}
```

---

## Broadphase

The broadphase culls potential collision pairs before narrowphase. Two approaches are planned:

### Spatial Hash (CPU fallback)

- Grid-based spatial partitioning
- Cell size = max collider diameter x 2
- O(n) average case for uniform distribution

### SAP / GPU-AABB (RDNA target)

- Sort Axis-Aligned Bounding Boxes along each axis
- Overlap test on sorted arrays
- Execute as Vulkan/DX12 compute dispatch
- Leverages wave32 on RDNA for parallel sorting

---

## Narrowphase

| Shape Pair | Algorithm | GPU? |
|------------|-----------|------|
| AABB vs AABB | SAT (Separating Axis Theorem) | ✅ RDNA compute |
| Sphere vs Sphere | Distance check | ✅ All tiers |
| Capsule vs Capsule | Axis-aligned sweep | ✅ RDNA compute |
| Convex vs Convex | GJK-EPA | 📋 Planned |

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

## ECS Integration

The `PhysicsSystem` reads `PhysicsBody` and `Transform` components, simulates one physics tick, and writes back updated `Transform` components:

```rust
impl System for PhysicsSystem {
    fn update(&mut self, world: &mut World, dt: f32) {
        // 1. Read all PhysicsBody + Transform pairs
        let bodies: Vec<(Entity, PhysicsBody, Transform)> = world
            .query_entities_with::<PhysicsBody, Transform>()
            .map(|e| (e, world.get::<PhysicsBody>(e).unwrap(), world.get::<Transform>(e).unwrap()))
            .collect();

        // 2. Run broadphase to find collision pairs
        let collisions = self.broadphase.find_candidates(&bodies);

        // 3. Run narrowphase for each pair
        let contacts = self.narrowphase.resolve_all(&bodies, &collisions);

        // 4. Solve constraints (impulse-based resolution)
        let impulses = self.constraint_solver.solve(&bodies, &contacts, dt);

        // 5. Integrate and write back Transform
        for (entity, body, _) in &bodies {
            self.integrator.step(body, dt, &impulses);
            let new_transform = Transform {
                position: body.position + body.linear_velocity * dt,
                ../* rotation from angular_velocity */
            };
            world.add_component(entity, new_transform);
        }

        // 6. Emit collision events
        for contact in &contacts {
            world.emit_event(CollisionEvent {
                entity_a: contact.a, entity_b: contact.b,
                normal: contact.normal, penetration: contact.penetration,
            });
        }
    }
}
```

---

## GPU Compute Kernels

### RDNA (WGSL / GLSL)

```glsl
// Broadphase: Spatial Hash on GPU
@group(0) @binding(0) var<storage, read_write> bodies: array<PhysicsBody>;
@group(0) @binding(1) var<storage, read_write> grid: array<u32>;
@group(0) @binding(2) var<uniform> params: Params;

@compute @workgroup_size(256)
fn broadphase(@builtin(global_invocation_id) gid: vec3u) {
    let body_idx = gid.x;
    if (body_idx >= params.body_count) { return; }
    let body = bodies[body_idx];
    let cell = hash_position(body.position, params.cell_size);
    grid[cell] = body_idx;
}
```

### ARM NEON Fallback

```rust
#[cfg(target_arch = "aarch64")]
unsafe fn broadphase_neon(bodies: &mut [PhysicsBody]) {
    // NEON intrinsics for parallel AABB overlap test
}
```

### RISC-V RVV Scalar

```rust
#[cfg(target_arch = "riscv64")]
unsafe fn broadphase_rvv(bodies: &mut [PhysicsBody]) {
    // RVV vectorized spatial hash
}
```

---

## Async Compute

Physics runs on a separate compute queue from the graphics queue:

- **RDNA**: Async compute hardware units execute physics while GPU renders
- **ARM**: Compute and graphics queues share the same GPU — serialized
- **RISC-V**: Software fallback, physics blocks render (no async hardware)

---

## Roadmap

### Short-term (1-3 months)
- [ ] Implement `PhysicsBody` component with all fields
- [ ] Build AABB broadphase (SAP) in CPU path
- [ ] Implement SAT for AABB vs AABB narrowphase
- [ ] Add semi-implicit Euler integrator
- [ ] ECS integration: read `PhysicsBody`, write `Transform`

### Mid-term (3-12 months)
- [ ] GPU broadphase: RDNA WGSL compute shader
- [ ] Sphere-sphere and capsule-capsule narrowphase
- [ ] Impulse-based constraint solver
- [ ] Collision event emission
- [ ] ARM NEON physics fallback
- [ ] RISC-V RVV scalar path

### Long-term (1-3 years)
- [ ] GJK-EPA for convex-convex collision
- [ ] Soft body / deformable physics
- [ ] Character controller with slope caching
- [ ] GPU-driven particle physics

### Experimental
- 💡 Neural collision prediction via NPU
- 💡 Procedural terrain deformation
- 💡 Real-time fluid simulation (SPH on GPU)

### Hardware-Specific
- **RDNA / AMD:** WGSL compute shaders, wave32 optimization, async compute queue
- **Moore Threads:** MUSA compute shaders, Vulkan 1.3 compute
- **ARM / Mobile:** NEON intrinsics, fixed-step simulation for power efficiency
- **RISC-V:** RVV vectorized spatial hash, software ray-cast fallback