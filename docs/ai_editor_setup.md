# Litt Engine AI Editor - Setup Guide

This guide explains how any AI system can use Litt Engine to build game editors.

## Overview

Litt Engine is headless-first, but includes tools for AI systems to build editors programmatically or via UI.

## Three Ways to Use

### 1. Web Editor (Recommended for Humans)
- **Location:** `editor/index.html`
- **How:** Open in any browser
- **Features:**
  - Visual entity hierarchy
  - Component inspector
  - 3D viewport
  - AI chat interface
  - Export to JSON

### 2. Python API (Recommended for AI Agents)
- **Location:** `python/litt_ai_editor/`
- **How:** Import and use programmatically
- **Features:**
  - Full entity/component system
  - Scene import/export
  - Event system
  - CLI interface

### 3. JSON Protocol (For Any Language)
- **Location:** `docs/ai_editor_protocol.md`
- **How:** Send JSON-RPC requests
- **Features:**
  - Language agnostic
  - WebSocket or HTTP
  - Full API coverage

## Getting Started

### For Python AI Agents
```python
from litt_ai_editor import Editor

# Create editor
editor = Editor()

# Build scene
player = editor.create_entity("player", position=(0, 0, 0))
editor.add_component(player, "mesh", {"model": "assets/player.glb"})
editor.add_component(player, "physics", {"type": "dynamic", "mass": 70})

# Export
editor.export_scene("my_game.json")
```

### For Browser Automation
```python
# Example: Using Selenium to drive the web editor
from selenium import webdriver

driver = webdriver.Chrome()
driver.get("file:///path/to/editor/index.html")

# Use AI chat
chat_input = driver.find_element("#ai-input")
chat_input.send_keys("create player at 0,0,0")
chat_input.submit()

# Export
driver.find_element("#btn-export").click()
```

### For Any Language via JSON-RPC
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

## AI Agent Workflow

### Step 1: Understand the API
Read [`docs/ai_editor_agent_guide.md`](docs/ai_editor_agent_guide.md) for complete documentation.

### Step 2: Choose Your Interface
- **Python API** if building in Python
- **JSON Protocol** if using any other language
- **Web Editor** if you need visual feedback

### Step 3: Build Your Scene
Use the API to create entities, add components, and export.

### Step 4: Import into Litt Engine
```bash
littcli import my_scene.json
```

## Natural Language Commands

The web editor accepts natural language:

| Command | Result |
|---------|--------|
| `create player` | Creates "player" entity |
| `create enemy at 10,5,0` | Creates with position |
| `add mesh to player` | Adds mesh component |
| `add physics with mass 70` | Adds physics config |
| `delete enemy_01` | Removes entity |
| `export scene to level.json` | Saves to file |

## Component Types

| Component | Purpose | Key Properties |
|-----------|---------|----------------|
| `mesh` | 3D model | model, material, visible |
| `physics` | Physics body | type, mass, friction |
| `light` | Illumination | type, color, intensity |
| `camera` | Viewport | fov, near, far |
| `script` | Behavior | path, params |
| `audio` | Sound | path, volume |

## Example: Complete Level

```python
from litt_ai_editor import Editor
import math

editor = Editor()

# Create ground
ground = editor.create_entity("ground", position=(0, -1, 0))
editor.add_component(ground, "mesh", {"model": "assets/ground.glb"})
editor.add_component(ground, "physics", {"type": "static"})

# Create player
player = editor.create_entity("player", position=(0, 1, 0))
editor.add_component(player, "mesh", {"model": "assets/player.glb"})
editor.add_component(player, "physics", {"type": "dynamic", "mass": 70})
editor.add_component(player, "script", {"path": "scripts/player.lua"})

# Create coins in a circle
for i in range(8):
    angle = (i / 8) * 2 * math.pi
    x = 5 * math.cos(angle)
    z = 5 * math.sin(angle)
    
    coin = editor.create_entity(f"coin_{i}", position=(x, 1, z))
    editor.add_component(coin, "mesh", {"model": "assets/coin.glb"})
    editor.add_component(coin, "script", {
        "path": "scripts/collectible.lua",
        "params": {"type": "coin", "value": 10}
    })

# Export
editor.export_scene("circle_level.json")
```

## Testing the Editor

### Run Python Examples
```bash
python python/examples/ai_editor_example.py
```

### Open Web Editor
```bash
# Just open the HTML file
start editor/index.html
```

### Run Unit Tests
```bash
pip install pytest
python -m pytest python/tests/
```

## API Reference

- **Editor Class:** [`python/litt_ai_editor/editor.py`](python/litt_ai_editor/editor.py)
- **C API:** [`include/litt_ai_editor.h`](include/litt_ai_editor.h)
- **Protocol:** [`docs/ai_editor_protocol.md`](docs/ai_editor_protocol.md)
- **Guide:** [`docs/ai_editor_agent_guide.md`](docs/ai_editor_agent_guide.md)

## Next Steps

1. Read the agent guide
2. Try the examples
3. Build your first scene
4. Contribute improvements

## Support

- Issues: GitHub Issues
- Documentation: `docs/`
- Examples: `python/examples/`
