//! Litt Engine ECS - Entity Component System
//!
//! High-performance ECS with archetype-based storage.
//!
//! # Core Types
//! - `Entity` - Unique identifier for game objects
//! - `Component` - Trait for data that can be attached to entities
//! - `World` - Container for all entities, components, and systems
//! - `Query` - Filter entities by component composition
//! - `System` - Trait for update logic

#![allow(clippy::missing_safety_intrinsic)]
#![allow(clippy::type_complexity)]

use std::collections::HashMap;
use std::any::{TypeId, Any};

// =============================================================================
// Core Types
// =============================================================================

/// Unique entity identifier
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Entity(u32);

impl Entity {
    #[inline]
    pub const fn new(id: u32) -> Self { Self(id) }

    #[inline]
    pub fn id(&self) -> u32 { self.0 }
}

impl std::fmt::Display for Entity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Entity({})", self.0)
    }
}

// =============================================================================
// Component Trait
// =============================================================================

/// Trait for all component types
pub trait Component: Send + Sync + 'static {}

// Blanket implementation
impl<T: Send + Sync + 'static> Component for T {}

/// Marker for clone-able components
pub trait CloneableComponent: Component + Clone {}
impl<T: Component + Clone> CloneableComponent for T {}

/// Marker for copy components
pub trait CopyComponent: Component + Copy + Clone {}
impl<T: Component + Copy + Clone> CopyComponent for T {}

/// Marker for POD components (suitable for GPU buffers)
pub trait PodComponent: Component + bytemuck::Pod + bytemuck::Zeroable {}
impl<T: Component + bytemuck::Pod + bytemuck::Zeroable> PodComponent for T {}

// =============================================================================
// Type-Erased Component Storage
// =============================================================================

/// Trait for any component (type-erased)
pub trait AnyComponent: Any + Send + Sync {
    fn as_any(&self) -> &dyn Any;
    fn as_any_mut(&mut self) -> &mut dyn Any;
}

impl<T: Component> AnyComponent for T {
    fn as_any(&self) -> &dyn Any { self }
    fn as_any_mut(&mut self) -> &mut dyn Any { self }
}

// =============================================================================
// Component Storage
// =============================================================================

/// Storage for components in the world
#[derive(Debug)]
pub struct ComponentStore {
    components: HashMap<u32, Box<dyn AnyComponent>>,
}

impl ComponentStore {
    pub fn new() -> Self {
        Self { components: HashMap::new() }
    }

    pub fn insert(&mut self, entity: Entity, component: Box<dyn AnyComponent>) {
        self.components.insert(entity.id(), component);
    }

    pub fn get(&self, entity: Entity) -> Option<&dyn AnyComponent> {
        self.components.get(&entity.id()).map(|b| b.as_ref())
    }

    pub fn get_mut(&mut self, entity: Entity) -> Option<&mut dyn AnyComponent> {
        self.components.get_mut(&entity.id()).map(|b| b.as_mut())
    }

    pub fn remove(&mut self, entity: Entity) -> Option<Box<dyn AnyComponent>> {
        self.components.remove(&entity.id())
    }

    #[inline]
    pub fn contains(&self, entity: Entity) -> bool {
        self.components.contains_key(&entity.id())
    }

    #[inline]
    pub fn len(&self) -> usize { self.components.len() }

    #[inline]
    pub fn is_empty(&self) -> bool { self.components.is_empty() }

    pub fn entity_ids(&self) -> impl Iterator<Item = Entity> + '_ {
        self.components.keys().map(|&id| Entity::new(id))
    }
}

impl Default for ComponentStore {
    fn default() -> Self { Self::new() }
}

// =============================================================================
// World
// =============================================================================

/// The main ECS world
pub struct World {
    component_stores: HashMap<TypeId, ComponentStore>,
    entities: Vec<Entity>,
    next_id: u32,
    systems: Vec<Box<dyn System>>,
}

impl World {
    pub fn new() -> Self {
        Self {
            component_stores: HashMap::new(),
            entities: Vec::new(),
            next_id: 0,
            systems: Vec::new(),
        }
    }

    /// Create a new entity
    pub fn create_entity(&mut self) -> Entity {
        let id = self.next_id;
        self.next_id += 1;
        let entity = Entity::new(id);
        self.entities.push(entity);
        entity
    }

    /// Create an entity with a component
    pub fn create_entity_with<T: Component>(&mut self, component: T) -> Entity {
        let entity = self.create_entity();
        self.add_component(entity, component);
        entity
    }

    /// Destroy an entity
    pub fn destroy_entity(&mut self, entity: Entity) {
        self.entities.retain(|&e| e != entity);
        for store in self.component_stores.values_mut() {
            store.remove(entity);
        }
    }

    /// Add a component to an entity
    pub fn add_component<T: Component>(&mut self, entity: Entity, component: T) {
        let type_id = TypeId::of::<T>();
        if !self.component_stores.contains_key(&type_id) {
            self.component_stores.insert(type_id, ComponentStore::new());
        }
        self.component_stores.get_mut(&type_id).unwrap().insert(entity, Box::new(component));
    }

    /// Get a component from an entity
    pub fn get_component<T: Component>(&self, entity: Entity) -> Option<&T> {
        let type_id = TypeId::of::<T>();
        self.component_stores.get(&type_id)?
            .get(entity)?
            .as_any()
            .downcast_ref::<T>()
    }

    /// Get a mutable component from an entity
    pub fn get_component_mut<T: Component>(&mut self, entity: Entity) -> Option<&mut T> {
        let type_id = TypeId::of::<T>();
        self.component_stores.get_mut(&type_id)?
            .get_mut(entity)?
            .as_any_mut()
            .downcast_mut::<T>()
    }

    /// Remove a component from an entity
    pub fn remove_component<T: Component>(&mut self, entity: Entity) {
        let type_id = TypeId::of::<T>();
        if let Some(store) = self.component_stores.get_mut(&type_id) {
            store.remove(entity);
        }
    }

    /// Check if entity has a component
    pub fn has_component<T: Component>(&self, entity: Entity) -> bool {
        let type_id = TypeId::of::<T>();
        self.component_stores.get(&type_id)
            .map(|store| store.contains(entity))
            .unwrap_or(false)
    }

    /// Query entities with component C1
    pub fn query_entities<C1: Component>(&self) -> impl Iterator<Item = Entity> + '_ {
        let type_id = TypeId::of::<C1>();
        self.component_stores.get(&type_id)
            .map(|store| store.entity_ids())
            .into_iter().flatten()
            .filter(move |entity| self.entities.contains(entity))
    }

    /// Query entities with both components C1 and C2
    pub fn query_entities_with<C1: Component, C2: Component>(&self) -> impl Iterator<Item = Entity> + '_ {
        let type_id1 = TypeId::of::<C1>();
        let type_id2 = TypeId::of::<C2>();
        let store1 = self.component_stores.get(&type_id1);
        let store2 = self.component_stores.get(&type_id2);

        match (store1, store2) {
            (Some(s1), Some(s2)) => {
                EitherIter::Filtered(s1.entity_ids().filter(move |entity| s2.contains(*entity)))
            }
            _ => EitherIter::Empty,
        }
    }

    /// Register a system
    pub fn add_system<S: System + 'static>(&mut self, system: S) -> &mut Self {
        self.systems.push(Box::new(system));
        self
    }

    /// Run all systems
    pub fn run_systems(&mut self, dt: f32) {
        for system in &mut self.systems {
            system.update(self, dt);
        }
    }

    /// Run systems in specified order
    pub fn run_systems_ordered(&mut self, dt: f32, order: &[&str]) {
        let mut ordered: Vec<&mut Box<dyn System>> = Vec::new();
        for name in order {
            for system in &mut self.systems {
                if system.name() == *name && !ordered.contains(&system) {
                    ordered.push(system);
                    break;
                }
            }
        }
        for system in &mut self.systems {
            if !ordered.contains(&system) {
                ordered.push(system);
            }
        }
        for system in ordered {
            system.update(self, dt);
        }
    }

    #[inline]
    pub fn entity_count(&self) -> usize { self.entities.len() }

    #[inline]
    pub fn entities(&self) -> impl Iterator<Item = Entity> + '_ {
        self.entities.iter().copied()
    }
}

impl Default for World {
    fn default() -> Self { Self::new() }
}

// =============================================================================
// System Trait
// =============================================================================

/// System trait for update logic
pub trait System: Send + Sync {
    fn update(&mut self, world: &mut World, dt: f32);
    fn name(&self) -> &str { "system" }
}

// =============================================================================
// System Group
// =============================================================================

/// Group of systems that run together
pub struct SystemGroup {
    systems: Vec<Box<dyn System>>,
}

impl SystemGroup {
    pub fn new() -> Self { Self { systems: Vec::new() } }

    pub fn add<S: System + 'static>(&mut self, system: S) -> &mut Self {
        self.systems.push(Box::new(system));
        self
    }

    pub fn run(&mut self, world: &mut World, dt: f32) {
        for system in &mut self.systems {
            system.update(world, dt);
        }
    }
}

impl Default for SystemGroup {
    fn default() -> Self { Self::new() }
}

// =============================================================================
// Query Iterators
// =============================================================================

enum EitherIter {
    Empty,
    Filtered(std::iter::Filter<std::iter::Map<std::collections::hash_map::IntoKeys<u32, Box<dyn AnyComponent>>, Entity>, fn(&Entity) -> bool>),
}

impl Iterator for EitherIter {
    type Item = Entity;
    fn next(&mut self) -> Option<Self::Item> {
        match self {
            EitherIter::Empty => None,
            EitherIter::Filtered(iter) => iter.next(),
        }
    }
}

// =============================================================================
// Re-exports
// =============================================================================

pub use bytemuck;

// Neural AI components
pub mod neural;
pub mod neural_system;

pub use neural::*;
pub use neural_system::*;

// =============================================================================
// Tests
// =============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default, Clone, Debug)]
    struct Position { x: f32, y: f32 }

    #[derive(Default, Clone, Debug)]
    struct Velocity { dx: f32, dy: f32 }

    struct MovementSystem { dt: f32 }

    impl System for MovementSystem {
        fn name(&self) -> &str { "movement" }
        fn update(&mut self, world: &mut World, _dt: f32) {
            for entity in world.entities() {
                if let (Some(pos), Some(vel)) = (
                    world.get_component::<Position>(entity),
                    world.get_component::<Velocity>(entity),
                ) {
                    world.add_component(entity, Position {
                        x: pos.x + vel.dx * self.dt,
                        y: pos.y + vel.dy * self.dt,
                    });
                }
            }
        }
    }

    #[test]
    fn test_create_entity() {
        let mut world = World::new();
        assert_eq!(world.create_entity().id(), 0);
        assert_eq!(world.create_entity().id(), 1);
    }

    #[test]
    fn test_add_get_component() {
        let mut world = World::new();
        let e = world.create_entity();
        world.add_component(e, Position { x: 1.0, y: 2.0 });
        let pos = world.get_component::<Position>(e).unwrap();
        assert!((pos.x - 1.0).abs() < 1e-6);
    }

    #[test]
    fn test_system() {
        let mut world = World::new();
        let e = world.create_entity();
        world.add_component(e, Position { x: 0.0, y: 0.0 });
        world.add_component(e, Velocity { dx: 1.0, dy: 0.0 });

        let mut movement = MovementSystem { dt: 1.0 };
        movement.update(&mut world, 1.0);

        let pos = world.get_component::<Position>(e).unwrap();
        assert!((pos.x - 1.0).abs() < 1e-6);
    }

    #[test]
    fn test_system_group() {
        let mut world = World::new();
        let e = world.create_entity();
        world.add_component(e, Position { x: 0.0, y: 0.0 });
        world.add_component(e, Velocity { dx: 1.0, dy: 0.0 });

        let mut group = SystemGroup::new();
        group.add(MovementSystem { dt: 1.0 });
        group.run(&mut world, 1.0);

        let pos = world.get_component::<Position>(e).unwrap();
        assert!((pos.x - 1.0).abs() < 1e-6);
    }
}