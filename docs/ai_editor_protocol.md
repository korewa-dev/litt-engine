# AI Editor - JSON Protocol

This document describes the JSON protocol for the Litt Engine AI Editor API.

## Request Format

All requests are JSON objects with a `method` field:

```json
{
  "jsonrpc": "2.0",
  "method": "create_entity",
  "params": {
    "name": "player",
    "position": [0, 0, 0]
  },
  "id": 1
}
```

## Response Format

```json
{
  "jsonrpc": "2.0",
  "result": {
    "success": true,
    "entity_id": 1
  },
  "id": 1
}
```

## Methods

### create_entity

Create a new entity.

**Params:**
```json
{
  "name": "player",
  "position": [0, 0, 0],
  "rotation": [0, 0, 0],
  "scale": [1, 1, 1]
}
```

**Response:**
```json
{
  "success": true,
  "entity_id": 1
}
```

### delete_entity

Delete an entity.

**Params:**
```json
{
  "entity_id": 1
}
```

**Response:**
```json
{
  "success": true
}
```

### set_position

Set entity position.

**Params:**
```json
{
  "entity_id": 1,
  "position": [5, 0, 0]
}
```

### add_component

Add a component to an entity.

**Params:**
```json
{
  "entity_id": 1,
  "type": "mesh",
  "config": {
    "model": "assets/player.glb"
  }
}
```

**Response:**
```json
{
  "success": true
}
```

### remove_component

Remove a component from an entity.

**Params:**
```json
{
  "entity_id": 1,
  "type": "mesh"
}
```

### export_scene

Export scene to JSON file.

**Params:**
```json
{
  "path": "output/scene.json"
}
```

**Response:**
```json
{
  "success": true,
  "path": "output/scene.json"
}
```

### import_scene

Import scene from JSON file.

**Params:**
```json
{
  "path": "input/scene.json"
}
```

### list_entities

List all entities.

**Response:**
```json
{
  "entities": [
    {
      "id": 1,
      "name": "player",
      "position": [0, 0, 0],
      "components": {"mesh": {...}}
    }
  ]
}
```

### get_entity

Get entity details.

**Params:**
```json
{
  "entity_id": 1
}
```

### find_by_name

Find entity by name.

**Params:**
```json
{
  "name": "player"
}
```

### find_by_tag

Find entities by tag.

**Params:**
```json
{
  "tag": "enemy"
}
```

### count_entities

Get entity count.

**Response:**
```json
{
  "count": 5
}
```

### version

Get API version.

**Response:**
```json
{
  "version": "1.0.0"
}
```

## Component Types

### mesh

3D model renderer.

**Config:**
```json
{
  "model": "path/to/model.glb",
  "material": "material_name",
  "visible": true,
  "cast_shadows": true,
  "receive_shadows": true
}
```

### physics

Physics body.

**Config:**
```json
{
  "type": "dynamic",
  "mass": 70.0,
  "friction": 0.7,
  "restitution": 0.3,
  "linear_damping": 0.1,
  "angular_damping": 0.1
}
```

**Types:**
- `static` - Not affected by physics
- `kinematic` - Moves but not affected by forces
- `dynamic` - Fully simulated

### light

Light source.

**Config:**
```json
{
  "type": "point",
  "color": [1.0, 0.95, 0.8],
  "intensity": 1.0,
  "range": 100.0,
  "cast_shadows": true
}
```

**Types:**
- `point` - Omnidirectional
- `directional` - Sun-like
- `spot` - Cone-shaped

### camera

Camera renderer.

**Config:**
```json
{
  "fov": 60,
  "near": 0.1,
  "far": 1000.0,
  "aspect": 1.777,
  "screenshot": false
}
```

### script

Script behavior.

**Config:**
```json
{
  "path": "scripts/player.lua",
  "params": {
    "max_health": 100
  }
}
```

### audio

Audio source.

**Config:**
```json
{
  "path": "audio/sound.ogg",
  "volume": 0.8,
  "loop": false
}
```

## Error Handling

All errors follow this format:

```json
{
  "success": false,
  "error": "Entity not found",
  "code": 404
}
```

**Error Codes:**
- `400` - Bad Request
- `404` - Not Found
- `500` - Internal Error

## Event System

### subscribe

Subscribe to event.

**Params:**
```json
{
  "event": "entity.created",
  "callback": "on_entity_created"
}
```

**Response:**
```json
{
  "handler_id": 1
}
```

### emit

Emit event.

**Params:**
```json
{
  "event": "entity.created",
  "data": {
    "entity_id": 1,
    "name": "player"
  }
}
```

## Example Workflow

### Complete Level Creation

```json
[
  {"jsonrpc": "2.0", "method": "create_entity", "params": {"name": "ground", "position": [0, -1, 0]}, "id": 1},
  {"jsonrpc": "2.0", "method": "add_component", "params": {"entity_id": 1, "type": "mesh", "config": {"model": "assets/plane.glb"}}, "id": 2},
  {"jsonrpc": "2.0", "method": "add_component", "params": {"entity_id": 1, "type": "physics", "config": {"type": "static"}}, "id": 3},
  {"jsonrpc": "2.0", "method": "create_entity", "params": {"name": "player", "position": [0, 0, 0]}, "id": 4},
  {"jsonrpc": "2.0", "method": "add_component", "params": {"entity_id": 2, "type": "mesh", "config": {"model": "assets/player.glb"}}, "id": 5},
  {"jsonrpc": "2.0", "method": "add_component", "params": {"entity_id": 2, "type": "physics", "config": {"type": "dynamic", "mass": 70}}, "id": 6},
  {"jsonrpc": "2.0", "method": "export_scene", "params": {"path": "level.json"}, "id": 7}
]
```

## Schema

Full JSON Schema available at `schemas/ai_editor.json`.
