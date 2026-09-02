# Litt Engine

> **NOTE:** This README accurately reflects current implementation status. See [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for details.

## What is Litt Engine?

AI-driven game engine designed for autonomous AI agents to build, control, and run fully-optimized games. The engine is headless-first with a C/C++ core, Python worldgen tooling, and a growing set of working games.

**Current state:** Header-heavy with partial implementations. Core math, OBJ loading, JSON parsing, and dither rendering are working. ECS, physics, renderer, audio, UI, and input are stubs awaiting implementation.

## AI Editor - Make Any AI Build Editors

Litt Engine now includes a complete **AI Editor** that enables any AI system to build game editors:

### 🌐 Web Editor
Open [`editor/index.html`](editor/index.html) in any browser for a Unity-like interface:
- Entity hierarchy panel
- Component inspector
- 3D viewport with grid
- AI chat for natural language commands
- Export/import JSON scenes
- Run/stop game simulation

### 🐍 Python API
```python
from litt_ai_editor import Editor

editor = Editor()
player = editor.create_entity("player", position=(0, 0, 0))
editor.add_component(player, "mesh", {"model": "assets/player.glb"})
editor.add_component(player, "physics", {"type": "dynamic", "mass": 70})
editor.export_scene("my_game.json")
```

### 📡 JSON Protocol
Full JSON-RPC protocol for any language. See [`docs/ai_editor_protocol.md`](docs/ai_editor_protocol.md).

### 🤖 AI Commands
Use natural language in the editor:
- "create player at 0,0,0"
- "add physics to player with mass 70"
- "export scene to game.json"

## Quick Links

| Resource | Link |
|----------|------|
| Main docs | [`docs/README.md`](docs/README.md) |
| Philosophy | [`docs/PHILOSOPHY.md`](docs/PHILOSOPHY.md) |
| Agent entry | [`docs/AGENT_ENTRY_POINTS.md`](docs/AGENT_ENTRY_POINTS.md) |
| Conventions | [`CONVENTIONS.md`](CONVENTIONS.md) |
| AI rules | [`Project/live/AI_RULES.md`](Project/live/AI_RULES.md) |
| Implementation status | [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md) |
| **AI Editor Guide** | [`docs/ai_editor_agent_guide.md`](docs/ai_editor_agent_guide.md) |
| **Web Editor** | [`editor/index.html`](editor/index.html) |
| **AI Editor API** | [`include/litt_ai_editor.h`](include/litt_ai_editor.h) |

## Project Layout

```
litt_engine/
├── editor/                    # 🌐 Web-based AI Editor
│   ├── index.html             # Editor UI
│   ├── editor.css             # Dark theme styles
│   ├── editor.js              # Editor engine
│   └── README.md              # Editor guide
├── python/                    # 🐍 Python AI Editor API
│   ├── litt_ai_editor/        # Editor package
│   ├── examples/              # Example scripts
│   ├── tests/                 # Unit tests
│   └── pyproject.toml         # Package config
├── include/                   # C/C++ headers
│   ├── litt_ai_editor.h       # AI Editor API
│   └── litt_ffi.h             # FFI bindings
├── docs/                      # Documentation
│   ├── ai_editor_api.md       # API reference
│   ├── ai_editor_protocol.md  # JSON-RPC protocol
│   └── ai_editor_agent_guide.md  # Agent guide
├── native/                    # C/C++ engine core
│   ├── littcore/              # Core modules (headers + some implementations)
│   │   ├── litt_math.h/cpp    # ✅ Vec2/3/4, Mat4, Quat, AABB, raycasts
│   │   ├── litt_ecs.h         # ⚠️ Header-only: archetype ECS, no systems
│   │   ├── litt_physics.h     # ⚠️ Header-only: rigid bodies, no solver
│   │   ├── litt_renderer.h    # ⚠️ Header-only: no Vulkan/DX12 backend
│   │   ├── litt_audio.h       # ⚠️ Header-only: clip/source structs
│   │   ├── litt_ui.h          # ⚠️ Header-only: no backend
│   │   ├── litt_input.h       # ⚠️ Header-only: no backend
│   │   ├── litt_world.c       # ✅ World simulation
│   │   ├── litt_world.cpp     # ✅ World simulation C++
│   │   ├── litt_dither.cpp    # ✅ Dithered 3D rendering
│   │   ├── litt_json.c        # ✅ JSON parser
│   │   ├── litt_obj.c         # ✅ OBJ loader
│   │   └── build.bat          # Build script
│   └── bin/                   # Built executables
├── tools/                     # Python tools
│   └── litt.py                # Pipeline CLI
├── template/                  # Project templates
│   ├── docs/                  # Documentation templates
│   ├── tools/                 # Tool templates
│   └── ai/                    # AI agent configs
├── Project/                   # Game projects
│   ├── live/                  # Live game project
│   ├── forge-final-e2e/       # Final e2e test
│   └── worldforge-demo/       # Worldgen demo
├── shaders/                   # GLSL shaders
│   ├── studio.vert.glsl
│   └── studio.frag.glsl
├── scripts/                   # Lua scripts
├── assets/                    # Game assets
├── logs/                      # Runtime logs
└── output/                    # Exported content
```

## Build (Native)

```bash
cd native
.\build.bat
```

## Run Tests

```bash
cd native/bin
littcore_tests.exe
```

## Run Editor

Open `editor/index.html` in any browser.

## AI Usage Examples

### Example 1: Create Player Character
```python
from litt_ai_editor import Editor

editor = Editor()

# Create entity
player = editor.create_entity("player", position=(0, 1, 0))

# Add components
editor.add_component(player, "mesh", {"model": "assets/hero.glb"})
editor.add_component(player, "physics", {
    "type": "dynamic",
    "mass": 70,
    "friction": 0.8
})
editor.add_component(player, "script", {"path": "scripts/player.lua"})

# Export
editor.export_scene("level.json")
```

### Example 2: Build Level with AI
```python
# AI generates level programmatically
editor = Editor()

# Ground plane
ground = editor.create_entity("ground", position=(0, -1, 0))
editor.add_component(ground, "mesh", {"model": "assets/plane.glb"})
editor.add_component(ground, "physics", {"type": "static"})

# Walls
for i in range(4):
    wall = editor.create_entity(f"wall_{i}", position=(0, 5, -10 + i*5))
    editor.add_component(wall, "mesh", {"model": "assets/wall.glb"})
    editor.add_component(wall, "physics", {"type": "static"})

# Coins
for i in range(10):
    coin = editor.create_entity(f"coin_{i}", position=(i*2, 1, 0))
    editor.add_component(coin, "mesh", {"model": "assets/coin.glb"})
    editor.add_component(coin, "script", {
        "path": "scripts/collectible.lua",
        "params": {"type": "coin", "value": 10}
    })

editor.export_scene("level.json")
```

## License

MIT License - See [LICENSE](LICENSE) for details.
