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

### Features (Complete List)

| Category | Subsystems |
|---|---|
| **Mathematics** | Vector3, Matrix4x4, Quaternion, Complex numbers, Radiometry, BRDF, Fresnel, Snell, Microfacet, SSS, Volumetric |
| **ECS** | Entity/Component system, Sparse set pools, Python/C# bindings |
| **Memory** | Linear, Stack, Pool allocators |
| **Rendering** | Forward & Deferred paths, Rasterization, Path Tracing, BVH, GPU Ray Tracing (DXR/RTX) |
| **Post-Processing** | SSAO, HDR, Tone Mapping, Bloom, DOF, Motion Blur, TAA, SSR, Volumetric Lighting |
| **Advanced Graphics** | SSAO, Reflections (SSR), Refractions (Snell), SSS, Volumetric clouds, Water, Decals, Skybox |
| **Physics** | Rigidbody dynamics, Constraint solving, Broad/narrow phase, Collision response |
| **Audio** | 3D audio engine, Reverb (FDN), Miniaudio/OpenAL backend |
| **Input** | Keyboard, Mouse, Gamepad (XInput/SDL), deadzone handling |
| **Animation** | Skeletal animation, State machines, Blend trees, GPU skinning |
| **Scripting** | Lua (C API), C# (Mono), Python (pybind11 embedding) |
| **World Generation** | Terrain, LOD, Noise (FastNoiseLite/libnoise), Chunk streaming, Biomes |
| **Asset Pipeline** | `.litt` format, asset_cook.py converter, mesh/texture/audio/shader/world data |
| **Editor Tools** | ImGui executable, viewport, hierarchy, inspector, gizmos (ImGuizmo), asset browser, play mode, console, AI chat |
| **Dependency Management** | vcpkg.json, Conanfile.py, submodules, GCC/Clang/MSVC, C++17/C11, Python 3.8+ |
| **Testing** | Unit tests (math/ECS/memory/BVH), Integration tests (asset/scene), Python/AI editor tests, C# serialization tests, benchmarks |
| **Performance** | Profiling (Tracy), Frame timing, Draw call/memory tracking, SIMD optimizations |
| **Portability** | Windows (MSVC/MinGW), Linux (GCC/Clang), macOS (Clang optional), Console abstraction, Android/iOS optional |
| **Release/Packaging** | CPack (NSIS/.deb/.pkg), Asset bundles, VERSION file, CHANGELOG.md |
| **AI Agent Integration** | `litt_agent` C API, Python module, JSON-RPC, spawn/perception/actions |
| **Headless Mode** | `LITT_HEADLESS` flag, no window/rendering, physics/audio/scripting still run |
| **Misc Features** | Character controller, Vehicle physics, Networking (UDP/TCP, replication), User-defined components, Plugin system (dlopen/LoadLibrary), Real-time undo/redo |

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