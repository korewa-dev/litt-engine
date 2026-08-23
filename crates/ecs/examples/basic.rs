//! ECS Example - Demonstrating Litt Engine ECS usage

use litt_ecs::*;
use litt_math::*;

// =============================================================================
// Component Definitions
// =============================================================================

/// Transform component - position, rotation, scale
#[derive(Clone, Debug, Default)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: f32,
    pub scale: Vec3,
}

/// Velocity component
#[derive(Clone, Debug, Default)]
pub struct Velocity {
    pub linear: Vec3,
    pub angular: f32,
}

/// Mesh component
#[derive(Clone, Debug, Default)]
pub struct Mesh {
    pub vertex_count: u32,
    pub index_count: u32,
}

/// Name component for debugging
#[derive(Clone, Debug)]
pub struct Name {
    pub value: String,
}

// =============================================================================
// Systems
// =============================================================================

/// Movement system - updates positions based on velocity
pub struct MovementSystem {
    pub dt: f32,
}

impl System for MovementSystem {
    fn name(&self) -> &str { "movement" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        let updates: Vec<_> = world
            .query_entities_with::<Transform, Velocity>()
            .into_iter()
            .filter_map(|entity| {
                let transform = world.get_component::<Transform>(entity)?.clone();
                let velocity = world.get_component::<Velocity>(entity)?.clone();
                Some((entity, transform, velocity))
            })
            .collect();

        for (entity, transform, velocity) in updates {
            let new_pos = transform.position + velocity.linear * self.dt;
            world.add_component(entity, Transform {
                position: new_pos,
                ..transform
            });
        }
    }
}

/// Debug system - prints entity information
pub struct DebugSystem;

impl System for DebugSystem {
    fn name(&self) -> &str { "debug" }

    fn update(&mut self, world: &mut World, _dt: f32) {
        let count = world.entity_count();
        println!("World has {} entities", count);
    }
}

fn main() {
    let mut world = World::new();

    // Spawn an entity with components
    let entity = world.create_entity();
    world.add_component(entity, Transform::default());
    world.add_component(entity, Velocity { linear: Vec3::new(1.0, 0.0, 0.0), angular: 0.0 });
    world.add_component(entity, Mesh::default());

    // Run systems
    let mut movement = MovementSystem { dt: 1.0 / 60.0 };
    let mut debug = DebugSystem;
    movement.update(&mut world, movement.dt);
    debug.update(&mut world, 0.0);

    println!("Entity transform after update: {:?}", world.get_component::<Transform>(entity));
    println!("Total entities: {}", world.entity_count());
}

