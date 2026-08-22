//! ECS Module for Litt Engine
//!
//! Entity Component System integration with GPU-accelerated physics.
//! Bridges template components (Transform, Player, Camera, etc.) with litt_physics.

use crate::template::components::*;
use litt_ecs::*;
use litt_physics::*;

// =============================================================================
// ECS Systems for Litt Engine
// =============================================================================

/// Movement system -- updates transforms based on velocity (legacy compat)
pub struct MovementSystem {
    pub dt: f32,
}

impl litt_ecs::System for MovementSystem {
    fn name(&self) -> &str { "movement" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        for entity in world.query_entities_with::<Transform, Velocity>() {
            if let (Some(transform), Some(velocity)) = (
                world.get_component::<Transform>(entity),
                world.get_component::<Velocity>(entity),
            ) {
                let new_pos = transform.position + velocity.linear * self.dt;
                world.add_component(entity, Transform {
                    position: new_pos,
                    ..transform.clone()
                });
            }
        }
    }
}

/// Camera system -- follows player entity
pub struct CameraSystem {
    pub dt: f32,
}

impl litt_ecs::System for CameraSystem {
    fn name(&self) -> &str { "camera" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        for player_entity in world.query_entities_with::<Player, Transform>() {
            if let (Some(_player), Some(player_transform)) = (
                world.get_component::<Player>(player_entity),
                world.get_component::<Transform>(player_entity),
            ) {
                for camera_entity in world.query_entities::<Camera>() {
                    if let Some(camera) = world.get_component::<Camera>(camera_entity) {
                        let offset = Vec3::new(0.0, 2.0, 5.0);
                        let target_pos = player_transform.position + offset;
                        world.add_component(camera_entity, Camera {
                            position: target_pos,
                            rotation: Vec2::new(0.0, 0.0),
                            ..camera.clone()
                        });
                    }
                }
            }
        }
    }
}

/// Light system -- animates light direction
pub struct LightSystem;

impl litt_ecs::System for LightSystem {
    fn name(&self) -> &str { "light" }

    fn update(&mut self, world: &mut World, dt: f32) {
        for entity in world.query_entities::<Light>() {
            if let Some(light) = world.get_component::<Light>(entity) {
                let new_dir = Vec3::new(
                    light.direction.0 + dt * 0.1,
                    light.direction.1,
                    light.direction.2,
                ).normalized();
                world.add_component(entity, Light {
                    direction: new_dir,
                    ..light.clone()
                });
            }
        }
    }
}

// =============================================================================
// ECS World Builder -- bridges template Transform with physics PhysicsTransform
// =============================================================================

/// Build an ECS world with physics-enabled entities.
///
/// Each physics-enabled entity gets both a template Transform (for rendering)
/// and a physics PhysicsTransform (for the physics system). The physics system
/// writes back to PhysicsTransform, and a sync system copies position to Transform.
pub fn build_world() -> World {
    let mut world = World::new();

    // Player -- dynamic sphere body, starts above ground
    let player = world.create_entity();
    world.add_component(player, Transform {
        position: Vec3::new(0.0, 3.0, 0.0),
        rotation: Quat::default(),
        scale: Vec3::new(1.0, 1.0, 1.0),
    });
    world.add_component(player, PhysicsTransform {
        position: Vec3::new(0.0, 3.0, 0.0),
        rotation: [0.0, 0.0, 0.0, 1.0],
        scale: Vec3::new(1.0, 1.0, 1.0),
    });
    world.add_component(player, PhysicsBodyECS::new(ColliderShape::sphere(0.5), 1.0));
    world.add_component(player, Velocity::new(Vec3::ZERO));
    world.add_component(player, Player::new());
    world.add_component(player, Mesh::default());

    // Camera
    let camera = world.create_entity();
    world.add_component(camera, Camera {
        position: Vec3::new(0.0, 2.0, 5.0),
        rotation: Vec2::new(0.0, 0.0),
        fov: core::f32::consts::PI / 3.0,
        near_plane: 0.1,
        far_plane: 100.0,
        aspect: 16.0 / 9.0,
        exposure: 1.0,
    });

    // Light
    let light = world.create_entity();
    world.add_component(light, Light {
        position: Vec3::new(0.0, 8.0, -5.0),
        direction: Vec3::new(0.0, -1.0, 0.5).normalized(),
        color: Vec3::new(1.0, 0.95, 0.9),
        intensity: 50.0,
        radius: 2.0,
        _pad: [0.0; 2],
    });

    // Ground plane -- static AABB body (immovable)
    let ground = world.create_entity();
    world.add_component(ground, Transform {
        position: Vec3::new(0.0, 0.0, 0.0),
        rotation: Quat::default(),
        scale: Vec3::new(10.0, 0.1, 10.0),
    });
    world.add_component(ground, PhysicsTransform {
        position: Vec3::new(0.0, 0.0, 0.0),
        rotation: [0.0, 0.0, 0.0, 1.0],
        scale: Vec3::new(10.0, 0.1, 10.0),
    });
    world.add_component(ground, PhysicsBodyECS::static_body(
        ColliderShape::aabb(Vec3::new(5.0, 0.05, 5.0)),
    ));
    world.add_component(ground, Velocity::new(Vec3::ZERO));
    world.add_component(ground, Mesh::default());
    world.add_component(ground, Material {
        albedo: Vec3::new(0.2, 0.2, 0.2),
        ..Default::default()
    });

    // Falling cubes -- staggered heights, dynamic AABB bodies
    for i in 0..5 {
        let cube = world.create_entity();
        let start_y = 3.0 + i as f32 * 1.5;
        let pos = Vec3::new(i as f32 * 2.0 - 4.0, start_y, 0.0);
        world.add_component(cube, Transform {
            position: pos,
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        });
        world.add_component(cube, PhysicsTransform {
            position: pos,
            rotation: [0.0, 0.0, 0.0, 1.0],
            scale: Vec3::new(1.0, 1.0, 1.0),
        });
        world.add_component(cube, PhysicsBodyECS::new(
            ColliderShape::aabb(Vec3::new(0.5, 0.5, 0.5)),
            1.0,
        ));
        world.add_component(cube, Velocity::new(Vec3::new(0.0, -1.0 - i as f32 * 0.3, 0.0)));
        world.add_component(cube, Mesh::default());
        world.add_component(cube, Material {
            albedo: Vec3::new(0.8, 0.2 + i as f32 * 0.12, 0.2),
            ..Default::default()
        });
    }

    // Rolling sphere -- starts with horizontal velocity
    let sphere = world.create_entity();
    let sphere_pos = Vec3::new(3.0, 1.0, 2.0);
    world.add_component(sphere, Transform {
        position: sphere_pos,
        rotation: Quat::default(),
        scale: Vec3::new(1.0, 1.0, 1.0),
    });
    world.add_component(sphere, PhysicsTransform {
        position: sphere_pos,
        rotation: [0.0, 0.0, 0.0, 1.0],
        scale: Vec3::new(1.0, 1.0, 1.0),
    });
    world.add_component(sphere, PhysicsBodyECS::new(
        ColliderShape::sphere(0.5),
        2.0,
    ));
    world.add_component(sphere, Velocity::new(Vec3::new(3.0, 0.0, 0.0)));
    world.add_component(sphere, Mesh::default());
    world.add_component(sphere, Material {
        albedo: Vec3::new(0.2, 0.6, 0.8),
        ..Default::default()
    });

    world
}

/// Sync system -- copies PhysicsTransform positions back to Transform for rendering.
/// Run this AFTER PhysicsSystem.update() each frame.
pub struct PhysicsTransformSyncSystem;

impl litt_ecs::System for PhysicsTransformSyncSystem {
    fn name(&self) -> &str { "physics_sync" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        for entity in world.query_entities_with::<PhysicsTransform, Transform>() {
            if let (Some(phys_tr), Some(render_tr)) = (
                world.get_component::<PhysicsTransform>(entity),
                world.get_component::<Transform>(entity),
            ) {
                world.add_component(entity, Transform {
                    position: phys_tr.position,
                    rotation: render_tr.rotation.clone(),
                    scale: render_tr.scale.clone(),
                });
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_build_world_has_physics_entities() {
        let world = build_world();
        assert!(world.entity_count() > 0);

        // First entity (player) should have PhysicsBodyECS
        let player = world.entities().next().unwrap();
        assert!(world.has_component::<PhysicsBodyECS>(player));
        assert!(world.has_component::<PhysicsTransform>(player));
    }

    #[test]
    fn test_physics_system_creation() {
        let system = PhysicsSystem::new();
        assert!((system.gravity.1 - (-9.81)).abs() < 1e-6);
        assert!((system.fixed_dt - 1.0 / 60.0).abs() < 1e-6);
        assert_eq!(system.substeps, 2);
    }

    #[test]
    fn test_physics_system_at_hz() {
        let sys = PhysicsSystem::at_hz(144.0);
        assert!((sys.fixed_dt - 1.0 / 144.0).abs() < 1e-6);
    }

    #[test]
    fn test_physics_system_with_timing() {
        let sys = PhysicsSystem::with_timing(1.0 / 120.0, 4);
        assert!((sys.fixed_dt - 1.0 / 120.0).abs() < 1e-6);
        assert_eq!(sys.substeps, 4);
    }

    #[test]
    fn test_velocity_component() {
        let vel = Velocity::new(Vec3::new(1.0, 2.0, 3.0));
        assert!((vel.linear.0 - 1.0).abs() < 1e-6);
        assert!((vel.linear.1 - 2.0).abs() < 1e-6);
        assert!(vel.angular == Vec3::ZERO);
    }

    #[test]
    fn test_collision_event() {
        use litt_ecs::Entity;
        let e1 = Entity::new(0);
        let e2 = Entity::new(1);
        let event = CollisionEvent {
            entity_a: e1,
            entity_b: e2,
            normal: Vec3::new(0.0, 1.0, 0.0),
            penetration: 0.01,
        };
        assert!(event.involves(e1));
        assert!(event.involves(e2));
        assert!(!event.involves(Entity::new(99)));
    }
}
