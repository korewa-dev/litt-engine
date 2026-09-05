<!-- REMOVED STACK NOTICE (CDR-007): This document remains as design reference for the C/C++ port (native/littcore). -->
# Litt Engine ECS Architecture

## Overview

The ECS (Entity Component System) architecture for Litt Engine provides a data-oriented approach to game object management, separating data (components) from behavior (systems).

## Module Structure

(C++ port lives under `native/littcore/`)

## Core Components

### Entity
- Unique identifier (u32) for game objects
- No data or behavior itself
- Created via `World::create_entity()`

### Component
- Plain data structures attached to entities
- Implement the `Component` trait (blanket impl for Send + Sync + ''static)
- Can be any type: structs, primitives, etc.

### System
- Pure logic/behavior
- Implements the `System` trait
- Operates on entities with specific components

## World API

| Method | Description |
|--------|-------------|
| `create_entity()` | Create a new entity |
| `add_component(entity, component)` | Attach component to entity |
| `get_component<T>(entity)` | Get component reference |
| `get_component_mut<T>(entity)` | Get mutable component reference |
| `remove_component<T>(entity)` | Remove component from entity |
| `has_component<T>(entity)` | Check if entity has component |
| `query_entities::<C>()` | Get entities with component C |
| `query_entities_with::<C1, C2>()` | Get entities with both components |
| `add_system(system)` | Register a system |
| `run_systems(dt)` | Run all systems |
| `entity_count()` | Get number of entities |

## Systems Provided

- **MovementSystem**: Updates transforms based on velocity
- **CameraSystem**: Follows player entity
- **LightSystem**: Animates light direction

## Integration

```cpp
// C++ usage
#include "litt_ecs.h"

World world;
Entity entity = world.create_entity();
world.add_component(entity, Transform{});
```

## Testing

```bash
native/build.bat test   # engine tests now live under native/
```

