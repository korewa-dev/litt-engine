//! Rigid body integrator
//!
//! Implements semi-implicit Euler integration per docs/physics/physics-system.md

use litt_math::Vec3;
use super::physics_body::PhysicsBody;

/// Semi-implicit Euler integrator -- stable for rigid body simulation
pub struct SemiImplicitEulerIntegrator;

impl SemiImplicitEulerIntegrator {
    pub fn new() -> Self { Self }

    /// Step a single physics body
    pub fn step(&self, body: &mut PhysicsBody, dt: f32, external_force: Vec3) {
        if body.is_static() { return; }

        let gravity = Vec3::new(0.0, -9.81 * body.gravity_scale, 0.0);
        let total_force = external_force + gravity * body.mass;
        let acceleration = if body.mass > 0.0 { total_force / body.mass } else { Vec3::ZERO };

        body.linear_velocity = (body.linear_velocity + acceleration * dt)
            * (1.0 - body.linear_damping * dt);

        let max_speed = 100.0;
        if body.linear_velocity.length() > max_speed {
            body.linear_velocity = body.linear_velocity.normalized() * max_speed;
        }

        body.angular_velocity = (body.angular_velocity + acceleration * dt)
            * (1.0 - body.angular_damping * dt);
    }

    /// Apply an impulse to a body
    pub fn apply_impulse(&self, body: &mut PhysicsBody, impulse: Vec3) {
        if body.is_static() { return; }
        body.linear_velocity = body.linear_velocity + impulse * body.inv_mass;
    }

    /// Integrate position from velocity
    pub fn integrate_position(&self, position: &mut Vec3, body: &PhysicsBody, dt: f32) {
        if body.is_static() { return; }
        *position = *position + body.linear_velocity * dt;
    }
}

impl Default for SemiImplicitEulerIntegrator {
    fn default() -> Self { Self::new() }
}

/// Constraint solver for collision impulse response
#[derive(Clone, Debug)]
pub struct ConstraintSolver {
    pub max_iterations: u32,
}

impl ConstraintSolver {
    pub fn new() -> Self { Self { max_iterations: 3 } }

    /// Solve a single contact constraint
    pub fn solve_contact(
        &self,
        body_a: &mut PhysicsBody,
        body_b: &mut PhysicsBody,
        contact_normal: Vec3,
        penetration: f32,
    ) {
        let rel_vel = body_a.linear_velocity - body_b.linear_velocity;
        let vel_along_normal = rel_vel.dot(contact_normal);

        if vel_along_normal > 0.0 { return; }

        let e = body_a.restitution.min(body_b.restitution);
        let j = -(1.0 + e) * vel_along_normal;
        let inv_mass_sum = body_a.inv_mass + body_b.inv_mass;
        if inv_mass_sum == 0.0 { return; }
        let impulse_mag = j / inv_mass_sum;

        let friction = (body_a.friction * body_b.friction).sqrt();
        let tangent = (rel_vel - contact_normal * vel_along_normal).normalized();
        let friction_impulse = tangent * impulse_mag * friction;

        let total_impulse = contact_normal * impulse_mag;

        if !body_a.is_static() {
            body_a.linear_velocity = body_a.linear_velocity + total_impulse * body_a.inv_mass;
            body_a.linear_velocity = body_a.linear_velocity - friction_impulse * body_a.inv_mass;
        }
        if !body_b.is_static() {
            body_b.linear_velocity = body_b.linear_velocity - total_impulse * body_b.inv_mass;
            body_b.linear_velocity = body_b.linear_velocity + friction_impulse * body_b.inv_mass;
        }

        const CORRECTION_PERCENT: f32 = 0.2;
        const SLOP: f32 = 0.01;
        let correction_mag = (penetration - SLOP).max(0.0) * CORRECTION_PERCENT / inv_mass_sum;
        let correction = contact_normal * correction_mag;

        if !body_a.is_static() {
            body_a.linear_velocity = body_a.linear_velocity - correction * body_a.inv_mass;
        }
        if !body_b.is_static() {
            body_b.linear_velocity = body_b.linear_velocity + correction * body_b.inv_mass;
        }
    }
}

impl Default for ConstraintSolver {
    fn default() -> Self { Self::new() }
}

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::physics_body::{PhysicsBody, ColliderShape};
    use litt_math::Vec3;

    #[test]
    fn test_gravity_integration() {
        let mut body = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        let mut pos = Vec3::new(0.0, 10.0, 0.0);
        let integ = SemiImplicitEulerIntegrator::new();

        assert!((body.linear_velocity.1 - 0.0).abs() < 1e-6);

        integ.step(&mut body, 1.0 / 60.0, Vec3::ZERO);
        assert!(body.linear_velocity.1 < -0.1);
        assert!(body.linear_velocity.1 > -1.0);

        integ.integrate_position(&mut pos, &body, 1.0 / 60.0);
        assert!(pos.1 < 10.0);
    }

    #[test]
    fn test_static_body_ignores_gravity() {
        let mut body = PhysicsBody::static_body(ColliderShape::aabb(Vec3::new(1.0, 1.0, 1.0)));
        let integ = SemiImplicitEulerIntegrator::new();

        integ.step(&mut body, 1.0 / 60.0, Vec3::ZERO);
        assert!((body.linear_velocity.1 - 0.0).abs() < 1e-6);
    }

    #[test]
    fn test_velocity_clamp() {
        let mut body = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        body.linear_velocity = Vec3::new(200.0, 200.0, 200.0);
        let integ = SemiImplicitEulerIntegrator::new();

        integ.step(&mut body, 1.0, Vec3::ZERO);
        assert!(body.linear_velocity.length() <= 100.0 + 1e-6);
    }

    #[test]
    fn test_impulse_application() {
        let mut body = PhysicsBody::new(ColliderShape::sphere(1.0), 2.0);
        let integ = SemiImplicitEulerIntegrator::new();

        integ.apply_impulse(&mut body, Vec3::new(0.0, 10.0, 0.0));
        assert!((body.linear_velocity.1 - 5.0).abs() < 1e-6);
    }

    #[test]
    fn test_constraint_solver_bounce() {
        let mut body_a = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        body_a.linear_velocity = Vec3::new(0.0, -5.0, 0.0);

        let mut body_b = PhysicsBody::static_body(ColliderShape::aabb(Vec3::new(10.0, 0.05, 10.0)));

        let solver = ConstraintSolver::new();
        let normal = Vec3::new(0.0, 1.0, 0.0);
        solver.solve_contact(&mut body_a, &mut body_b, normal, 0.01);

        assert!(body_a.linear_velocity.1 > 0.0);
    }

    #[test]
    fn test_constraint_solver_no_resolve_moving_apart() {
        let mut body_a = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        body_a.linear_velocity = Vec3::new(0.0, 5.0, 0.0);

        let mut body_b = PhysicsBody::new(ColliderShape::sphere(0.5), 1.0);
        body_b.linear_velocity = Vec3::new(0.0, -5.0, 0.0);

        let solver = ConstraintSolver::new();
        let normal = Vec3::new(0.0, 1.0, 0.0);
        solver.solve_contact(&mut body_a, &mut body_b, normal, 0.01);

        assert!((body_a.linear_velocity.1 - 5.0).abs() < 1e-6);
    }
}
