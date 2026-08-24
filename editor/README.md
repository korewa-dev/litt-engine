# Litt Editor

A Unity/Godot-like game editor for Litt Engine with integrated chat system.
Pure C++17, Vulkan-based rendering, cross-platform (Windows/Linux/Android).

## Features

- **Scene Viewport**: 3D viewport with orbit camera, grid, and transform gizmo
- **Hierarchy Panel**: Tree view of all scene nodes with selection
- **Inspector Panel**: Edit node properties, transforms, and components
- **Chat System**: Integrated chat similar to DeepSeek Harness for AI assistance
- **Toolbar**: Transform tools (Select, Move, Rotate, Scale)
- **Undo/Redo**: Full history of editor actions
- **Cross-platform**: Windows, Linux, and Android support

## Architecture

```
native/
├── littcore/
│   ├── litt_math.h       # C++ math library (Vec2/3/4, Mat4, Quat, AABB)
│   ├── litt_ecs.h        # Entity Component System
│   ├── litt_scene.h      # Scene graph and management
│   ├── litt_world.h      # World simulation
│   ├── litt_physics.h    # Physics system
│   ├── litt_input.h      # Input handling
│   ├── litt_profiler.h   # Performance profiling
│   └── ...               # Other core libraries
├── litteditor.h          # Editor header
├── litteditor.cpp        # Editor implementation
├── game.cpp              # Game entry point
├── build.bat             # Windows build script
└── build.sh              # Linux build script
```

## Building

### Windows

```bash
cd native
.\build.bat
```

### Linux

```bash
cd native
chmod +x build.sh
./build.sh
```

### Android

Requires Android NDK. Configure in build script.

## Running

### Editor

```bash
# Windows
native\bin\windows\LittEditor.exe

# Linux
./native/bin/linux/LittEditor
```

### Game

```bash
# Run with default scene
native\bin\windows\game.exe

# Run with specific scene
native\bin\windows\game.exe --scene Project\live\assets\scenes\world.lscn.json
```

### CLI Validator

```bash
native\bin\windows\littcli.exe validate Project\live --frames 30
```

## Chat Commands

Type commands in the editor chat panel:

| Command | Description |
|---------|-------------|
| `/help` | Show available commands |
| `/status` | Show editor status (FPS, draw calls, etc.) |
| `/load <file>` | Load a scene file |
| `/save [file]` | Save current scene |
| `/reset` | Reset to default scene |
| `/clear` | Clear chat history |
| `/undo` | Undo last action |
| `/redo` | Redo last action |
| `/select <name>` | Select node by name |
| `/delete <name>` | Delete a node |
| `/add <type>` | Add new node |
| `/grid` | Toggle grid visibility |
| `/gizmo` | Toggle gizmo visibility |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `W` | Move tool |
| `R` | Rotate tool |
| `S` | Scale tool |
| `Q` | Select tool |
| `Delete` | Delete selected |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `G` | Toggle grid |
| `Right Mouse` | Orbit camera |
| `Middle Mouse` | Pan camera |
| `Scroll` | Zoom camera |

## Design Philosophy

1. **Unity-like Layout**: Familiar panels (hierarchy, inspector, scene view)
2. **Integrated Chat**: AI assistance built directly into the editor
3. **Lightweight**: Minimal dependencies, fast startup
4. **Cross-platform**: C++ core works on Windows, Linux, Android
5. **AI-First**: Designed for both humans and AI agents

## Project Structure

```
litt-engine/
├── native/                   # C++ core and editor
│   ├── littcore/            # Core libraries
│   ├── game.cpp             # Game entry point
│   ├── litteditor.cpp       # Editor implementation
│   ├── build.bat            # Windows build
│   └── build.sh             # Linux build
├── editor/                   # Editor documentation
│   ├── README.md
│   └── QUICKSTART.md
├── template/                 # World generation (Python)
│   └── tools/
│       └── worldgen/
├── Project/                  # Generated games
└── docs/                     # Documentation
```

## Future Work

- [ ] Vulkan rendering backend for scene view
- [ ] Asset browser and pipeline
- [ ] Animation editor
- [ ] Physics debug visualization
- [ ] Multi-window support
- [ ] Plugin system
- [ ] Visual shader editor
- [ ] Particle editor
- [ ] Timeline editor
- [ ] Android native app integration

## License

See [LICENSE](../LICENSE) for details.
