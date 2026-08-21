//! Physics system for the Litt Engine
//!
//! Implements rigid-body dynamics with gravity, collisions, and impulse response.
//!
//! Components:
//! - PhysBody: physical body properties (mass, restitution, friction)
//! - Velocity: linear and angular velocity
//! - Collider: simple sphere/box collider for collision detection
//!
//! System:
//! - PhysicsSystem: runs at fixed timestep, integrates velocities,
//!   detects and resolves collisions

use litt_ecs::*;
use litt_math::{Vec3, Bbox};
use crate::template::components::Transform;

// =============================================================================
// Physics Components
// =============================================================================

/// Physical body properties
#[derive(Clone, Debug)]
pub struct PhysBody {
    pub mass: f32,
    pub restitution: f32, // bounciness [0, 1]
    pub friction: f32,    // friction coefficient
    pub is_static: bool,
    pub layers: u32,      // collision layers
}

impl PhysBody {
    pub fn new(mass: f32) -> Self {
        Self {
            mass,
            restitution: 0.5,
            friction: 0.5,
            is_static: mass <= 0.0,
            layers: 0xFFFFFFFF,
        }
    }

    pub fn static_body() -> Self {
        Self {
            mass: 0.0,
            restitution: 0.5,
            friction: 0.5,
            is_static: true,
            layers: 0xFFFFFFFF,
        }
    }

    pub fn inverse_mass(&self) -> f32 {
        if self.is_static { 0.0 } else { 1.0 / self.mass }
    }
}

impl Default for PhysBody {
    fn default() -> Self { Self::new(1.0) }
}

/// Linear and angular velocity
#[derive(Clone, Debug, Default)]
pub struct Velocity {
    pub linear: Vec3,
    pub angular: Vec3, // not fully implemented yet
}

impl Velocity {
    pub fn new(linear: Vec3) -> Self {
        Self { linear, ..Default::default() }
    }
}

/// Collider shape for physics
#[derive(Clone, Debug)]
pub enum ColliderShape {
    Sphere { radius: f32 },
    Box { half_extent: Vec3 },
}

impl ColliderShape {
    pub fn sphere(radius: f32) -> Self { Self::Sphere { radius } }
    pub fn box_collider(half_extent: Vec3) -> Self { Self::Box { half_extent } }
}

/// Collision collider component
#[derive(Clone, Debug)]
pub struct Collider {
    pub shape: ColliderShape,
    pub enabled: bool,
}

impl Collider {
    pub fn sphere(radius: f32) -> Self {
        Self { shape: ColliderShape::sphere(radius), enabled: true }
    }

    pub fn box_collider(half_extent: Vec3) -> Self {
        Self { shape: ColliderShape::box_collider(half_extent), enabled: true }
    }
}

impl Default for Collider {
    fn default() -> Self { Self::sphere(0.5) }
}

// =============================================================================
// Physics System
// =============================================================================

/// Main physics system
pub struct PhysicsSystem {
    pub gravity: Vec3,
    pub fixed_dt: f32,
    pub substeps: u32,
}

impl PhysicsSystem {
    /// Default 60 Hz physics
    pub fn new() -> Self {
        Self {
            gravity: Vec3::new(0.0, -9.81, 0.0),
            fixed_dt: 1.0 / 60.0,
            substeps: 2,
        }
    }

    /// Physics at a specific Hz rate (e.g. 144, 240)
    pub fn at_hz(hz: f32) -> Self {
        Self {
            gravity: Vec3::new(0.0, -9.81, 0.0),
            fixed_dt: 1.0 / hz,
            substeps: 2,
        }
    }

    /// Physics matching a display refresh rate
    pub fn at_refresh_rate(hz: f32, substeps: u32) -> Self {
        Self {
            gravity: Vec3::new(0.0, -9.81, 0.0),
            fixed_dt: 1.0 / hz,
            substeps,
        }
    }

    /// Custom timestep and substeps
    pub fn with_timing(fixed_dt: f32, substeps: u32) -> Self {
        Self {
            gravity: Vec3::new(0.0, -9.81, 0.0),
            fixed_dt,
            substeps,
        }
    }

    /// Custom gravity, default timing
    pub fn with_gravity(gravity: Vec3) -> Self {
        Self { gravity, ..Default::default() }
    }

    /// Set the physics timestep directly
    pub fn set_fixed_dt(&mut self, fixed_dt: f32) {
        self.fixed_dt = fixed_dt;
    }

    /// Set the number of substeps
    pub fn set_substeps(&mut self, substeps: u32) {
        self.substeps = substeps;
    }
}

impl Default for PhysicsSystem { Self::new() }

impl System for PhysicsSystem {
    fn name(&self) -> &str { "physics" }

    fn update(&mut self, world: &mut World, dt: f32) {
        // Use fixed timestep for deterministic physics
        let substeps = self.substeps.max(1) as f32;
        let sub_dt = self.fixed_dt;

        // For each substep
        for _ in 0..self.substeps {
            self.step(world, sub_dt);
        }
    }
}

impl PhysicsSystem {
    fn step(&mut self, world: &mut World, dt: f32) {
        // Collect all physics entities
        let mut bodies: Vec<(Entity, PhysBody, Velocity, Transform)> = Vec::new();

        for entity in world.query_entities_with::<PhysBody, Velocity, Transform>() {
            if let (Some(body), Some(vel), Some(tr)) = (
                world.get_component::<PhysBody>(entity),
                world.get_component::<Velocity>(entity),
                world.get_component::<Transform>(entity),
            ) {
                bodies.push((entity, body.clone(), vel.clone(), tr.clone()));
            }
        }

        // Apply forces and integrate
        for (entity, body, mut vel, transform) in &mut bodies {
            if body.is_static { continue; }

            // Apply gravity
            vel.linear = vel.linear + self.gravity * dt;

            // Update position
            let new_pos = transform.position + vel.linear * dt;

            // Update component
            world.add_component(*entity, Transform {
                position: new_pos,
                rotation: transform.rotation.clone(),
                scale: transform.scale.clone(),
            });
            world.add_component(*entity, Velocity {
                linear: vel.linear.clone(),
                angular: vel.angular.clone(),
            });
        }

        // Collision detection and response
        self.resolve_collisions(world, &bodies, dt);
    }

    fn resolve_collisions(&mut self, world: &mut World, bodies: &[(Entity, PhysBody, Velocity, Transform)], dt: f32) {
        let n = bodies.len();
        for i in 0..n {
            for j in (i + 1)..n {
                let (_, body_i, vel_i, trans_i) = &bodies[i];
                let (_, body_j, vel_j, trans_j) = &bodies[j];

                if self.sphere_sphere_collision(trans_i, body_i, trans_j, body_j) {
                    self.resolve_collision(world, bodies[i].0, bodies[j].0, body_i, body_j, vel_i, vel_j);
                }
            }
        }

        // Check ground collisions
        for (entity, body, vel, trans) in bodies {
            if body.is_static { continue; }
            if trans.position.1 < 0.0 {
                // Ground collision
                let mut new_vel = vel.linear.clone();
                new_vel.1 = -new_vel.1 * body.restitution;
                new_vel.0 *= 1.0 - body.friction;
                new_vel.2 *= 1.0 - body.friction;

                world.add_component(*entity, Velocity {
                    linear: new_vel,
                    angular: vel.angular.clone(),
                });

                // Snap to ground
                world.add_component(*entity, Transform {
                    position: Vec3::new(trans.position.0, 0.0, trans.position.2),
                    rotation: trans.rotation.clone(),
                    scale: trans.scale.clone(),
                });
            }
        }
    }

    fn sphere_sphere_collision(
        &self,
        trans_a: &Transform,
        body_a: &PhysBody,
        trans_b: &Transform,
        body_b: &PhysBody,
    ) -> bool {
        let pos_a = trans_a.position;
        let pos_b = trans_b.position;
        let radius_a = 0.5 * trans_a.scale.0.max(trans_a.scale.1).max(trans_a.scale.2);
        let radius_b = 0.5 * trans_b.scale.0.max(trans_b.scale.1).max(trans_b.scale.2);

        let dist = (pos_b - pos_a).length();
        dist < (radius_a + radius_b)
    }

    fn resolve_collision(
        &self,
        world: &mut World,
        entity_a: Entity,
        entity_b: Entity,
        body_a: &PhysBody,
        body_b: &PhysBody,
        vel_a: &Velocity,
        vel_b: &Velocity,
    ) {
        let pos_a = match world.get_component::<Transform>(entity_a) {
            Some(t) => t.position,
            None => return,
        };
        let pos_b = match world.get_component::<Transform>(entity_b) {
            Some(t) => t.position,
            None => return,
        };

        // Collision normal
        let normal = (pos_b - pos_a).normalized();

        // Relative velocity
        let rel_vel = vel_a.linear - vel_b.linear;
        let vel_normal = rel_vel.dot(normal);

        // Don't resolve if moving apart
        if vel_normal > 0.0 { return; }

        // Coefficient of restitution
        let e = body_a.restitution.min(body_b.restitution);

        // Impulse scalar
        let j = -(1.0 + e) * vel_normal;
        let inv_mass_sum = body_a.inverse_mass() + body_b.inverse_mass();
        if inv_mass_sum == 0.0 { return; }
        let impulse = j / inv_mass_sum;

        // Apply impulse
        let impulse_vec = normal * impulse;

        if !body_a.is_static {
            let mut new_vel_a = vel_a.linear.clone();
            new_vel_a = new_vel_a + impulse_vec * body_a.inverse_mass();
            world.add_component(entity_a, Velocity {
                linear: new_vel_a,
                angular: vel_a.angular.clone(),
            });
        }

        if !body_b.is_static {
            let mut new_vel_b = vel_b.linear.clone();
            new_vel_b = new_vel_b - impulse_vec * body_b.inverse_mass();
            world.add_component(entity_b, Velocity {
                linear: new_vel_b,
                angular: vel_b.angular.clone(),
            });
        }
    }
}

// =============================================================================
// Simple Physics Controller
// =============================================================================

/// Simple 3D physics controller (wasd + space/shift)
#[derive(Clone, Debug)]
pub struct PhysicsController {
    pub forward: bool,
    pub backward: bool,
    pub left: bool,
    pub right: bool,
    pub jump: bool,
    pub crouch: bool,
    pub sprint: bool,
}

impl PhysicsController {
    pub fn new() -> Self {
        Self {
            forward: false,
            backward: false,
            left: false,
            right: false,
            jump: false,
            crouch: false,
            sprint: false,
        }
    }

    pub fn apply_force(&self, world: &mut World, entity: Entity, dt: f32) {
        let speed = 5.0;
        let jump_force = 5.0;

        if let (Some(body), Some(vel), Some(rot)) = (
            world.get_component::<PhysBody>(entity),
            world.get_component::<Velocity>(entity),
            world.get_component::<Transform>(entity),
        ) {
            if body.is_static { return; }

            let mut dir = Vec3::ZERO;
            if self.forward { dir.2 -= 1.0; }
            if self.backward { dir.2 += 1.0; }
            if self.left { dir.0 -= 1.0; }
            if self.right { dir.0 += 1.0; }
            dir = dir.normalized() * speed;

            // Apply directional force
            let mut new_vel = vel.linear.clone();
            new_vel.0 += dir.0 * dt;
            new_vel.2 += dir.2 * dt;

            // Jump
            if self.jump && vel.linear.1 < 0.1 {
                new_vel.1 += jump_force;
            }

            world.add_component(entity, Velocity {
                linear: new_vel,
                angular: vel.angular.clone(),
            });
        }
    }
}

impl Default for PhysicsController { Self::new() }

// =============================================================================
// Test
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_physics_system_creation() {
        let system = PhysicsSystem::new();
        assert!((system.gravity.1 - (-9.81)).abs() < 1e-6);
        assert!((system.fixed_dt - 1.0 / 60.0).abs() < 1e-6);
    }

    #[test]
    fn test_physics_at_hz() {
        let sys = PhysicsSystem::at_hz(144.0);
        assert!((sys.fixed_dt - 1.0 / 144.0).abs() < 1e-6);
        
        let sys240 = PhysicsSystem::at_hz(240.0);
        assert!((sys240.fixed_dt - 1.0 / 240.0).abs() < 1e-6);
    }

    #[test]
    fn test_physics_at_refresh_rate() {
        let sys = PhysicsSystem::at_refresh_rate(165.0, 4);
        assert!((sys.fixed_dt - 1.0 / 165.0).abs() < 1e-6);
        assert_eq!(sys.substeps, 4);
    }

    #[test]
    fn test_physics_with_timing() {
        let sys = PhysicsSystem::with_timing(1.0 / 120.0, 3);
        assert!((sys.fixed_dt - 1.0 / 120.0).abs() < 1e-6);
        assert_eq!(sys.substeps, 3);
    }

    #[test]
    fn test_set_fixed_dt() {
        let mut sys = PhysicsSystem::new();
        sys.set_fixed_dt(1.0 / 240.0);
        assert!((sys.fixed_dt - 1.0 / 240.0).abs() < 1e-6);
    }

    #[test]
    fn test_set_substeps() {
        let mut sys = PhysicsSystem::new();
        sys.set_substeps(4);
        assert_eq!(sys.substeps, 4);
    }

    #[test]
    fn test_sphere_sphere_collision() {
        let system = PhysicsSystem::new();

        let trans_a = Transform {
            position: Vec3::new(0.0, 0.5, 0.0),
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        };
        let body_a = PhysBody::new(1.0);

        let trans_b = Transform {
            position: Vec3::new(0.0, 1.5, 0.0),
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        };
        let body_b = PhysBody::new(1.0);

        assert!(system.sphere_sphere_collision(&trans_a, &body_a, &trans_b, &body_b));
    }

    #[test]
    fn test_no_collision() {
        let system = PhysicsSystem::new();

        let trans_a = Transform {
            position: Vec3::new(0.0, 0.5, 0.0),
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        };
        let body_a = PhysBody::new(1.0);

        let trans_b = Transform {
            position: Vec3::new(5.0, 5.0, 5.0),
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        };
        let body_b = PhysBody::new(1.0);

        assert!(!system.sphere_sphere_collision(&trans_a, &body_a, &trans_b, &body_b));
    }
}