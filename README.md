# Litt Engine – AI-First, Headless Game Engine

![Litt Engine](https://raw.githubusercontent.com/korewa-dev/litt-engine/main/docs/images/logo.png)

## Litt Engine – AI-First, Headless Game Engine

A headless-first, AI-driven game engine designed for building custom editors and game experiences. Litt Engine provides the core subsystems (math, rendering, physics, scripting, asset pipeline) as header-only and inline template implementations, enabling any AI system to use it to build game editors, tools, and games.

### Philosophy

- **AI-First**: Designed from the ground up for AI agents to drive content creation, world generation, and editor automation.
- **Headless-First**: No window or rendering required by default — physics, audio, scripting, and game logic run fully headless. Rendering is optional via RHI backends (Vulkan, DirectX 12).
- **Polyglot**: Full support for C++, Python (pybind11), C# (Mono), and Lua scripting.
- **Editor-Friendly**: Every subsystem is usable from an editor context; the editor folder is an **EXAMPLE/DEMO** showing how to build custom editors on top of Litt Engine, not a shipped editor itself.

### Architecture

```text
┌─────────────────────────────────────┐
│           Python / C# / Lua           │  ← Scripting API
└───────────────────────┬─────────────┘
                        │ JSON-RPC / WebSocket
┌─────────────────────────────────────┐
│          Editor Backend (FastAPI)     │  ← Web UI ↔ Engine
│  editor/backend/app.py                │
└───────────────────────┬─────────────┘
                        │ WebSockets
┌─────────────────────────────────────┐
│           Three.js Web Editor         │  ← UI/Interaction
│  editor/index.html, editor/editor.js  │
└───────────────────────┬─────────────┘
                        │ C FFI / pybind11
┌─────────────────────────────────────┐
│            C++ Core                   │  ← Engine Subsystems
│  litt_math, litt_ecs, litt_rhi,     │
│  litt_renderer, litt_pathtracer,    │
│  litt_physics, litt_audio, etc.     │
└───────────────────────┬─────────────┘
                        │ C Headers
┌─────────────────────────────────────┐
│           C Runtime (Standard)        │  ← malloc, thread, file I/O
└─────────────────────────────────────┘
```

### Capability Status

Litt Engine is under active stabilization. Treat APIs that compile as interfaces,
not proof that every advertised backend or advanced subsystem is production-ready.

| Area | Current status |
|---|---|
| Math / core containers | Implemented and covered by native regression tests |
| ECS | Implemented core entity/component storage with generation validation |
| Generated-game C runtime | Primary tested gameplay contract |
| C++ Game facade | Partial compatibility layer; being aligned with the C runtime |
| Scene graph | Implemented hierarchy/TRS core; persistence is not yet implemented |
| Software renderer | Partial, primarily exercised on Windows/headless fallback paths |
| Vulkan / DX12 / GPU ray tracing | Experimental or incomplete; do not assume production backend availability |
| Physics | Experimental AABB rigid-body solver; no full CCD/angular/manifold solver |
| Audio | Partial; platform/backend coverage varies |
| Python / C# / Lua | Mixed/experimental integration; verify the specific binding before relying on it |
| Networking / save-load / advanced post FX | Experimental/partial APIs, not production-complete |
| World generation | Active and tested, with ongoing placement/runtime-parity hardening |
| Editor | Example/demo tooling, not a shipped production editor |

For the most reliable game-building path, use `GAME_BUILD_PROTOCOL.md` and run
the repository validation/tests before declaring a generated game complete.

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine

# Install dependencies via vcpkg (recommended)
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/install-vcpkg.sh

# Build the engine (Release mode)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DLITT_ENABLE_PYTHON=ON -DLITT_ENABLE_CSHARP=ON -DLITT_ENABLE_VULKAN=ON -DLITT_ENABLE_DX12=ON
cmake --build . --config Release

# Or use Conan
conan install . --build=missing

# Run examples
./litt_examples/ai_scene_creation.py
```

### Python API

```python
import litt_engine as le

# Initialize engine
engine = le.Engine()

# Create a scene
scene = engine.create_scene()

# Add a cube
cube = scene.add_cube(position=[0, 0, 0], size=1.0)

# Run headless simulation
engine.run(headless=True)

# Or with display
engine.run(headless=False)
```

### C# API (Mono)

```csharp
using LittEngine;

// Initialize
var engine = new Engine();

// Create scene
var scene = engine.CreateScene();

// Add cube
var cube = scene.AddCube(position: new float3(0, 0, 0), size: 1.0f);

// Run
engine.Run(headless: true);
```

### License

MIT License. See `LICENSE` for details.

### References

- `docs/PHILOSOPHY.md` – Project philosophy and design goals
- `docs/ARCHITECTURE.md` – Overall layering diagram
- `docs/AGENT_ENTRY_POINTS.md` – AI agent interaction patterns
- `docs/IMPLEMENTATION_STATUS.md` – Current progress tracking
- `docs/ai_editor_agent_guide.md` – AI agent usage guide
- `docs/world_generation.md` – Custom world generator guide
- `Project/live/AI_RULES.md` – AI-specific rules for code generation

---

## Quick Start

```bash
# Minimal build
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Run the web editor demo
python -m http.server 8080  # From editor/ directory
# Then open http://localhost:8080
```

## Documentation

All documentation is available in the `docs/` directory:
- `docs/README.md` – This file
- `docs/BUILD.md` – Detailed build steps
- `docs/API.md` – C, C++, Python, C# API reference
- `docs/ai_editor_agent_guide.md` – For AI agents
- `docs/world_generation.md` – How to create custom generators
- `docs/ai/README.md` – AI subsystem documentation
- `docs/ai/npu-rules.md` – NPU/AMD AGS/FSR 3.1.5/FidelityFX Denoiser rules
- `docs/ai/npu-support.md` – NPU support verification
- `docs/ARCHITECTURE.md` – Architecture overview
- `docs/PHILOSOPHY.md` – Project philosophy
- `docs/IMPLEMENTATION_STATUS.md` – Current implementation status
- `docs/AGENT_ENTRY_POINTS.md` – AI agent entry points
- `docs/EDITOR_TOOLS.md` – Editor tools documentation
- `docs/ASSET_PIPELINE.md` – Asset pipeline specification
- `docs/RENDERING.md` – Rendering system details
- And many more...

## Contact

- GitHub: https://github.com/korewa-dev/litt-engine
- Issues: https://github.com/korewa-dev/litt-engine/issues