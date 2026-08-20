//! ECS Module for Litt Engine
//!
//! Entity Component System integration for the engine.

use crate::template::components::*;
use litt_ecs::*;

// =============================================================================
// ECS Systems for Litt Engine
// =============================================================================

/// Movement system - updates transforms based on velocity
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

/// Camera system - updates camera based on player input
pub struct CameraSystem {
    pub dt: f32,
}

impl litt_ecs::System for CameraSystem {
    fn name(&self) -> &str { "camera" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        // Camera follows player
        for player_entity in world.query_entities_with::<Player, Transform>() {
            if let (Some(player), Some(player_transform)) = (
                world.get_component::<Player>(player_entity),
                world.get_component::<Transform>(player_entity),
            ) {
                // Find camera entity
                for camera_entity in world.query_entities::<Camera>() {
                    if let Some(camera) = world.get_component::<Camera>(camera_entity) {
                        // Update camera position to follow player
                        let offset = Vec3::new(0.0, 2.0, 5.0);
                        let target_pos = player_transform.position + offset;
                        world.add_component(camera_entity, Camera {
                            position: target_pos,
                            rotation: player.rotation,
                            ..camera.clone()
                        });
                    }
                }
            }
        }
    }
}

/// Light system - updates light positions
pub struct LightSystem;

impl litt_ecs::System for LightSystem {
    fn name(&self) -> &str { "light" }

    fn update(&mut self, world: &mut World, dt: f32) {
        // Rotate directional light slowly
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
// ECS World Builder
// =============================================================================

/// Build an ECS world with default entities
pub fn build_world() -> World {
    let mut world = World::new();

    // Create player
    let player = world.create_entity();
    world.add_component(player, Transform {
        position: Vec3::new(0.0, 1.0, 0.0),
        rotation: Quat::from_axis_angle(Vec3::Y, 0.0),
        scale: Vec3::new(1.0, 1.0, 1.0),
    });
    world.add_component(player, Player::new());
    world.add_component(player, Mesh::default());

    // Create camera
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

    // Create light
    let light = world.create_entity();
    world.add_component(light, Light {
        position: Vec3::new(0.0, 8.0, -5.0),
        direction: Vec3::new(0.0, -1.0, 0.5).normalized(),
        color: Vec3::new(1.0, 0.95, 0.9),
        intensity: 50.0,
        radius: 2.0,
        _pad: [0.0; 2],
    });

    // Create ground plane
    let ground = world.create_entity();
    world.add_component(ground, Transform {
        position: Vec3::new(0.0, 0.0, 0.0),
        rotation: Quat::default(),
        scale: Vec3::new(10.0, 0.1, 10.0),
    });
    world.add_component(ground, Mesh::default());
    world.add_component(ground, Material {
        albedo: Vec3::new(0.2, 0.2, 0.2),
        ..Default::default()
    });

    // Create some cubes
    for i in 0..5 {
        let cube = world.create_entity();
        world.add_component(cube, Transform {
            position: Vec3::new(i as f32 * 2.0 - 4.0, 0.5, 0.0),
            rotation: Quat::default(),
            scale: Vec3::new(1.0, 1.0, 1.0),
        });
        world.add_component(cube, Mesh::default());
        world.add_component(cube, Material {
            albedo: Vec3::new(0.8, 0.2, 0.2),
            ..Default::default()
        });
    }

    world
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_build_world() {
        let world = build_world();
        assert!(world.entity_count() > 0);
    }
}
