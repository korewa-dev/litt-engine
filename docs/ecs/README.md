<!-- REMOVED STACK NOTICE (CDR-007): This document remains as design reference for the C/C++ port (native/littcore). -->
# ECS Documentation

Entity Component System architecture and reference.

## Files

| File | Content |
|------|---------|
| [architecture.md](./architecture.md) | ECS module structure, World API |
| [reference.md](./reference.md) | Complete component and system reference |
| [components.md](./components.md) | Template component definitions |

## Quick Start

```cpp
#include "litt_ecs.h"

World world;
Entity entity = world.create_entity();
world.add_component(entity, Transform{});
world.add_component(entity, Velocity{});

for (auto e : world.query_entities_with<Transform, Velocity>()) {
    auto& t = world.get_component<Transform>(e);
    auto& v = world.get_component<Velocity>(e);
    t.position += v.linear * dt;
}
```

See [reference.md](./reference.md) for the complete component and system table.

