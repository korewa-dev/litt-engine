# Litt Editor Example

A **browser-based example editor** demonstrating how to build editor tools on top of Litt Engine. This is a starting point - you extend or replace it for your own tools.

## Quick Start

1. Open `index.html` in any modern browser (or serve via `python -m http.server 8080`)
2. Use the AI Chat panel to describe what you want to build
3. Or use the UI to manually create entities

## Features (Demo)

- **Entity Hierarchy** - Tree view of all game objects
- **Component Inspector** - View and edit entity components
- **3D Viewport** - Visual scene preview with grid
- **Transform Tools** - Move, rotate, scale entities
- **AI Chat** - Natural language commands to build scenes
- **Export/Import** - Save scenes as JSON
- **Run/Stop** - Test your game simulation

## Commands

### AI Chat Commands (Demo)
Type in the console or AI chat:
- `create player` - Create a new entity named "player"
- `create enemy at 10,5,0` - Create entity with position
- `add mesh to player` - Add mesh component
- `delete enemy_01` - Remove entity
- `export scene to level.json` - Save scene

### Keyboard Shortcuts
| Key | Action |
|-----|--------|
| V | Select tool |
| W | Move tool |
| E | Rotate tool |
| R | Scale tool |
| G | Toggle grid |
| Delete | Delete selected |

## File Structure

```
editor/
├── index.html     # Main UI
├── editor.css     # Dark theme styles
└── editor.js      # Editor engine
```

## Using with Litt Engine

Export your scene and load it in Litt Engine:

```python
from litt import Editor

# Import scene
editor = Editor()
editor.import_scene("level.json")

# Run
editor.run()
```

## AI Integration

Any AI can use this editor via:
1. Direct browser automation (Selenium, Puppeteer)
2. JSON-RPC protocol (see `docs/ai_editor_protocol.md`)
3. Python API (see `python/litt_ai_editor/`)

## Customization

Edit `editor.css` to change the theme, or `editor.js` to add new tools.

## License

Same as Litt Engine.
