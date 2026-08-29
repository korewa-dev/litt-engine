# Litt Engine AI Editor

AI-friendly editor API for Litt Engine. Build game editors with any AI system.

## Quick Start

```bash
# Install
pip install -e .

# Use CLI
litt-editor create --name player --position 0 0 0
litt-editor add-component --entity 1 --type mesh --config mesh.json
litt-editor export --output scene.json
```

## Python API

```python
from litt_ai_editor import Editor

editor = Editor()

# Create entity
entity = editor.create_entity("player", position=(0, 0, 0))
entity_id = entity['entity_id']

# Add components
editor.add_component(entity_id, "mesh", {"model": "assets/player.glb"})
editor.add_component(entity_id, "physics", {"type": "dynamic", "mass": 70})

# Export
editor.export_scene("scene.json")
```

## Features

- **Cross-platform**: Works on Windows, macOS, Linux
- **Language bindings**: Python, C++, C
- **JSON protocol**: Easy integration with any AI
- **Component system**: Modular entity composition
- **Scripting support**: Lua/Python scripts
- **Event system**: Async event handling

## Installation

### From source

```bash
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine/python
pip install -e .
```

### From PyPI (coming soon)

```bash
pip install litt-ai-editor
```

## Building from source

```bash
# Prerequisites
pip install pybind11 pytest
python -m pytest tests/
```

## CLI Reference

```
litt-editor create    Create new entity
litt-editor delete    Delete entity
litt-editor list      List all entities
litt-editor add-component    Add component to entity
litt-editor remove-component Remove component from entity
litt-editor export     Export scene to JSON
litt-editor import     Import scene from JSON
litt-editor render     Render single frame
litt-editor version    Show version
```

## Examples

See [examples/](examples/) for complete examples.

## Documentation

See [docs/ai_editor_api.md](../docs/ai_editor_api.md) for full API reference.

## License

MIT
