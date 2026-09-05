# Litt Engine AI Editor - Agent Guide

## Overview

This guide teaches AI agents how to use the Litt Engine AI Editor to build game editors and scenes.

## Quick Start

```python
from litt_ai_editor import Editor

# Create editor
editor = Editor()

# Create entity
result = editor.create_entity("player", position=(0, 0, 0))
entity_id = result['entity_id']

# Add components
editor.add_component(entity_id, "mesh", {"model": "assets/player.glb"})
editor.add_component(entity_id, "physics", {"type": "dynamic", "mass": 70})

# Export
editor.export_scene("Project/my_game/scene.json")
```

## Entity System

### Creating Entities

```python
# Basic entity
player = editor.create_entity("player")

# With position
enemy = editor.create_entity("enemy", position=(10, 0, 0))

# With full transform
prop = editor.create_entity("crate", 
    position=(5, 1, 3),
    rotation=(0, 45, 0),
    scale=(1, 1, 1)
)
```

### Entity Properties

| Property | Type | Description |
|----------|------|-------------|
| `id` | int | Unique entity identifier |
| `name` | str | Display name |
| `position` | tuple | (x, y, z) in world space |
| `rotation` | tuple | (x, y, z) Euler angles in degrees |
| `scale` | tuple | (x, y, z) scale factors |
| `components` | dict | All attached components |
| `tags` | list | Tags for grouping |
| `active` | bool | Whether entity is active |

## Component System

### Available Components

| Component | Purpose | Key Properties |
|-----------|---------|----------------|
| `transform` | Position, rotation, scale | (default) |
| `mesh` | 3D model renderer | model, material, visible |
| `physics` | Physics body | type, mass, friction |
| `light` | Light source | type, color, intensity |
| `camera` | Camera renderer | fov, near, far |
| `script` | Script behavior | path, params |
| `audio` | Audio source | path, volume |
| `collision` | Collision shape | type, size |

### Adding Components

```python
# Mesh component
editor.add_component(entity_id, "mesh", {
    "model": "assets/box.glb",
    "material": "default",
    "visible": True
})

# Physics component
editor.add_component(entity_id, "physics", {
    "type": "dynamic",  # static, kinematic, dynamic
    "mass": 10.0,
    "friction": 0.7,
    "restitution": 0.3
})

# Light component
editor.add_component(entity_id, "light", {
    "type": "point",  # point, directional, spot
    "color": [1.0, 0.95, 0.8],
    "intensity": 1.0,
    "range": 10.0
})

# Camera component
editor.add_component(entity_id, "camera", {
    "fov": 60,
    "near": 0.1,
    "far": 1000,
    "aspect": 16/9
})
```

### Component Types

**Mesh Component:**
```python
{
    "model": "path/to/model.glb",
    "material": "material_name",
    "visible": True,
    "cast_shadows": True,
    "receive_shadows": True
}
```

**Physics Component:**
```python
{
    "type": "dynamic",  # static, kinematic, dynamic
    "mass": 1.0,
    "friction": 0.7,
    "restitution": 0.3,
    "linear_damping": 0.1,
    "angular_damping": 0.1
}
```

**Light Component:**
```python
{
    "type": "point",      # point, directional, spot
    "color": [1.0, 1.0, 1.0],
    "intensity": 1.0,
    "range": 100.0,
    "cast_shadows": True
}
```

**Camera Component:**
```python
{
    "fov": 60,
    "near": 0.1,
    "far": 1000.0,
    "aspect": 1.777,
    "screenshot": False
}
```

## Scene Management

### Exporting Scenes

```python
# Export to JSON
editor.export_scene("Project/my_game/scene.json")

# Export with path
editor.export_scene("assets/level.json")
```

### Importing Scenes

```python
# Import from JSON
editor.import_scene("Project/my_game/scene.json")

# Import and replace
editor.import_scene("new_level.json")
```

### Scene Format

```json
{
  "version": "1.0.0",
  "entities": [
    {
      "id": 1,
      "name": "player",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "scale": [1, 1, 1],
      "components": {
        "mesh": {
          "model": "assets/player.glb"
        },
        "physics": {
          "type": "dynamic",
          "mass": 70
        }
      },
      "tags": ["player", "character"],
      "active": true
    }
  ]
}
```

## Working with Multiple Entities

### Creating a Group

```python
# Create player and camera
player = editor.create_entity("player", position=(0, 0, 0))
camera = editor.create_entity("camera", position=(0, 5, -10))

# Link them
editor.add_component(camera['entity_id'], "script", {
    "path": "scripts/camera_follow.lua",
    "params": {"target": "player"}
})
```

### Tagging Entities

```python
# Add tags
player = editor.create_entity("player", position=(0, 0, 0))
editor.add_component(player['entity_id'], "transform")
# In a real implementation, you'd have a tag component
player['tags'] = ["player", "character", "alive"]

# Find by tag
players = editor.find_by_tag("player")
```

## Building Common Game Objects

### Creating a Player Character

```python
def create_player(editor, name="player"):
    """Create a complete player character"""
    # Entity
    result = editor.create_entity(name, position=(0, 1, 0))
    entity_id = result['entity_id']
    
    # Visual
    editor.add_component(entity_id, "mesh", {
        "model": "assets/characters/hero.glb",
        "material": "hero_material"
    })
    
    # Physics
    editor.add_component(entity_id, "physics", {
        "type": "dynamic",
        "mass": 70.0,
        "friction": 0.8,
        "can_sleep": True
    })
    
    # Animation
    editor.add_component(entity_id, "animation", {
        "skeleton": "assets/characters/hero_skel.glb",
        "default_state": "idle"
    })
    
    # Script
    editor.add_component(entity_id, "script", {
        "path": "scripts/player_controller.lua"
    })
    
    return entity_id

# Usage
player_id = create_player(editor, "main_player")
```

### Creating a Level

```python
def create_level(editor, grid_size=10, platform_size=4):
    """Create a grid of platforms"""
    import math
    
    for x in range(-grid_size, grid_size + 1):
        for z in range(-grid_size, grid_size + 1):
            # Height based on sine wave
            y = math.sin(x * 0.3) * math.cos(z * 0.3) * 2
            
            platform = editor.create_entity(
                f"platform_{x}_{z}",
                position=(x * platform_size, y - 1, z * platform_size)
            )
            
            editor.add_component(platform['entity_id'], "mesh", {
                "model": "assets/platform.glb"
            })
            editor.add_component(platform['entity_id'], "physics", {
                "type": "static"
            })
    
    # Add lights
    editor.create_entity("sun", position=(50, 100, 50))
    editor.add_component(editor.count_entities(), "light", {
        "type": "directional",
        "color": [1.0, 0.95, 0.8],
        "intensity": 1.5
    })
    
    # Add camera
    editor.create_entity("camera", position=(0, 20, -30))
    editor.add_component(editor.count_entities(), "camera", {
        "fov": 60,
        "near": 0.1,
        "far": 1000
    })

# Usage
create_level(editor, grid_size=5, platform_size=5)
editor.export_scene("level.json")
```

### Creating Collectibles

```python
def create_coin(editor, position, index=0):
    """Create a collectible coin"""
    result = editor.create_entity(f"coin_{index}", position=position)
    
    editor.add_component(result['entity_id'], "mesh", {
        "model": "assets/coin.glb",
        "material": "gold"
    })
    
    editor.add_component(result['entity_id'], "physics", {
        "type": "static"
    })
    
    editor.add_component(result['entity_id'], "script", {
        "path": "scripts/collectible.lua",
        "params": {"type": "coin", "value": 10}
    })
    
    return result['entity_id']

# Create multiple coins
for i in range(20):
    angle = (i / 20) * 2 * math.pi
    x = 10 * math.cos(angle)
    z = 10 * math.sin(angle)
    create_coin(editor, (x, 1, z), i)
```

## Best Practices

### 1. Use Descriptive Names

```python
# Good
player = editor.create_entity("player_main", position=(0, 0, 0))
enemy_patrol = editor.create_entity("enemy_patrol_01", position=(10, 0, 0))

# Bad
e1 = editor.create_entity("entity1", position=(0, 0, 0))
e2 = editor.create_entity("thing", position=(10, 0, 0))
```

### 2. Organize with Tags

```python
# Tag related entities
editor.add_tag(player_id, "player")
editor.add_tag(enemy1_id, "enemy")
editor.add_tag(enemy2_id, "enemy")

# Find all enemies
enemies = editor.find_by_tag("enemy")
```

### 3. Use Component Defaults

```python
# Let the engine use defaults
editor.add_component(entity_id, "physics")  # Uses default dynamic

# Only override what you need
editor.add_component(entity_id, "physics", {"mass": 50})
```

### 4. Validate Before Export

```python
# Check all entities have meshes
for entity in editor.list_entities():
    if "mesh" not in entity.get("components", {}):
        print(f"Warning: Entity {entity['name']} has no mesh!")

# Export
editor.export_scene("validated_scene.json")
```

## Common Patterns

### Spawn System

```python
def create_spawn(editor, name, position, rotation):
    """Create a spawn point"""
    result = editor.create_entity(f"spawn_{name}", position=position)
    editor.set_rotation(result['entity_id'], rotation)
    editor.add_component(result['entity_id'], "spawn_point", {
        "name": name,
        "team": None
    })
    return result['entity_id']

# Usage
spawn_player = create_spawn(editor, "player", (0, 0, 0), (0, 0, 0))
spawn_enemy = create_spawn(editor, "enemy", (10, 0, 0), (0, 180, 0))
```

### Trigger Zone

```python
def create_trigger(editor, name, position, size):
    """Create a trigger zone"""
    result = editor.create_entity(f"trigger_{name}", position=position)
    editor.add_component(result['entity_id'], "trigger", {
        "size": size,
        "event": "on_enter",
        "duration": -1  # -1 for infinite
    })
    return result['entity_id']

# Usage
trigger = create_trigger(editor, "damage_zone", (0, 2, 0), (5, 5, 5))
```

### Dialogue System

```python
def create_dialogue(editor, npc_id, lines):
    """Add dialogue to NPC"""
    editor.add_component(npc_id, "dialogue", {
        "lines": lines,
        "default": "greeting"
    })

# Usage
create_dialogue(editor, merchant_id, {
    "greeting": "Welcome to my shop!",
    "farewell": "Come back soon!",
    "trade": "What can I sell you?"
})
```

## Debugging

### Check Entity Status

```python
def debug_entity(editor, entity_id):
    """Debug an entity"""
    entity = editor.get_entity(entity_id)
    if not entity:
        print(f"Entity {entity_id} not found")
        return
    
    print(f"Entity: {entity['name']}")
    print(f"  Position: {entity['position']}")
    print(f"  Components: {list(entity['components'].keys())}")
    
    # Check for missing components
    if 'mesh' not in entity['components']:
        print("  Warning: No mesh component!")
```

### List All Entities

```python
def list_all(editor):
    """List all entities with details"""
    entities = editor.list_entities()
    print(f"Total entities: {len(entities)}")
    
    for e in entities:
        comps = list(e.get('components', {}).keys())
        print(f"  [{e['id']}] {e['name']}: {', '.join(comps)}")
```

## Next Steps

1. **Explore the examples**: See `python/examples/` for complete demos
2. **Read the API docs**: See `docs/ai_editor_api.md`
3. **Run tests**: `python -m pytest python/tests/`
4. **Build the C extension**: See `python/README.md`

## Resources

- [API Reference](docs/ai_editor_api.md)
- [Python Examples](python/examples/)
- [Test Suite](python/tests/)
