# Litt Engine AI Editor API

## Overview

The Litt Engine AI Editor API provides a standardized interface for any AI system to interact with the engine and build editors. This API is:

- **Language-agnostic**: Works with Python, C++, or any language with FFI support
- **JSON-based**: Uses JSON for configuration and data exchange
- **Modular**: Components can be added/removed dynamically
- **Scriptable**: Supports Lua/Python scripting for custom behavior

## Quick Start

```python
from litt_ai_editor import Editor

# Create editor
editor = Editor()

# Create entity
entity = editor.create_entity(
    name="player",
    position=(0, 0, 0),
    rotation=(0, 0, 0),
    scale=(1, 1, 1)
)

# Add components
editor.add_component(entity, "mesh", {
    "model": "assets/player.glb",
    "material": "default"
})

editor.add_component(entity, "physics", {
    "type": "dynamic",
    "mass": 1.0
})

# Export scene
editor.export_scene("Project/my_game/scene.json")
```

## Installation

### Python

```bash
pip install litt_ai-editor
```

### C++

```cpp
#include "litt_ai_editor.h"

litt_editor_t* editor = litt_editor_create(nullptr);
```

## API Reference

### Editor Lifecycle

| Function | Description |
|----------|-------------|
| `Editor()` | Create new editor instance |
| `editor.close()` | Destroy editor and free resources |
| `editor.version()` | Get API version string |

### Scene Management

| Method | Description |
|--------|-------------|
| `create_entity(name, position, rotation, scale)` | Create new entity |
| `delete_entity(entity_id)` | Remove entity |
| `set_position(entity_id, pos)` | Set entity position |
| `set_rotation(entity_id, rot)` | Set entity rotation |
| `set_scale(entity_id, scale)` | Set entity scale |
| `get_entity(entity_id)` | Get entity properties |
| `list_entities()` | List all entities |

### Components

| Component | Description | Key Properties |
|-----------|-------------|----------------|
| `transform` | Position, rotation, scale | (default) |
| `mesh` | 3D model renderer | model, material, visible |
| `physics` | Physics body | type, mass, friction |
| `light` | Light source | type, color, intensity |
| `camera` | Camera renderer | fov, near, far |
| `script` | Script behavior | path, params |
| `audio` | Audio source | path, volume |

### Assets

| Method | Description |
|--------|-------------|
| `load_asset(path, type)` | Load asset from disk |
| `export_scene(path)` | Export scene to JSON |
| `import_scene(path)` | Import scene from JSON |

### Renderer

| Method | Description |
|--------|-------------|
| `render_frame(width, height, path)` | Render single frame |
| `set_camera(config)` | Configure camera |
| `add_light(config)` | Add light source |

### Query

| Method | Description |
|--------|-------------|
| `count_entities()` | Get entity count |
| `count_components(type)` | Get component count |
| `find_by_name(name)` | Find entity by name |
| `find_by_tag(tag)` | Find entities by tag |

### Scripting

| Method | Description |
|--------|-------------|
| `execute_script(path, params)` | Run script file |
| `evaluate(expression)` | Evaluate expression |

## Event System

```python
def on_entity_created(entity_id):
    print(f"Created entity {entity_id}")

editor.subscribe("entity.created", on_entity_created)
```

## Configuration

```json
{
  "renderer": {
    "backend": "vulkan",
    "width": 1920,
    "height": 1080,
    "vsync": true
  },
  "physics": {
    "enabled": true,
    "gravity": -9.81
  },
  "assets": {
    "base_path": "assets/",
    "cache_size_mb": 256
  }
}
```

## Examples

### Creating a Player Entity

```python
editor = Editor()

# Create player
player = editor.create_entity("player", (0, 0, 0))

# Add mesh
editor.add_component(player, "mesh", {
    "model": "assets/player.glb"
})

# Add physics
editor.add_component(player, "physics", {
    "type": "dynamic",
    "mass": 70.0
})

# Add script
editor.add_component(player, "script", {
    "path": "scripts/player.lua"
})

editor.export_scene("Project/my_game/scene.json")
```

### Building a Level

```python
editor = Editor()

# Ground plane
ground = editor.create_entity("ground", (0, -1, 0), (0, 0, 0), (100, 1, 100))
editor.add_component(ground, "mesh", {"model": "assets/plane.glb"})
editor.add_component(ground, "physics", {"type": "static"})

# Walls
for i, pos in enumerate([(10, 2, 0), (-10, 2, 0), (0, 2, 10), (0, 2, -10)]):
    wall = editor.create_entity(f"wall_{i}", pos, (0, 0, 0), (1, 4, 1))
    editor.add_component(wall, "mesh", {"model": "assets/wall.glb"})
    editor.add_component(wall, "physics", {"type": "static"})

editor.export_scene("level.json")
```

## Error Handling

All methods return result objects:

```python
result = editor.create_entity("player", (0, 0, 0))
if not result.success:
    print(f"Error: {result.error}")
```

## Contributing

1. Fork the repository
2. Create feature branch
3. Add tests
4. Submit pull request

## License

MIT
