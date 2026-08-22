# ECS Documentation

Entity Component System architecture and reference.

## Files

| File | Content |
|------|---------|
| [architecture.md](./architecture.md) | ECS crate structure, World API |
| [reference.md](./reference.md) | Complete component and system reference |
| [components.md](./components.md) | Template component definitions |

## Quick Start

```rust
use litt_ecs::*;

let mut world = World::new();
let entity = world.create_entity();
world.add_component(entity, Transform { position: Vec3::new(0.0, 0.0, 0.0), ..Default::default() });
world.add_component(entity, Velocity { linear: Vec3::new(1.0, 0.0, 0.0) });

for e in world.query_entities_with::<Transform, Velocity>() {
    let mut t = world.get_component_mut::<Transform>(e).unwrap();
    let v = world.get_component::<Velocity>(e).unwrap();
    t.position += v.linear * dt;
}
```

See [reference.md](./reference.md) for the complete component and system table.

