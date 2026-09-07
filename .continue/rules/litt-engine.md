# Litt Engine - AI Agent Rules & Implementation Guide

## What This Engine IS

Litt Engine is a **game engine** — not an editor. It is designed to be **driven by AI agents** to build games. The engine is **headless-first**: physics, audio, scripting, and game logic run without rendering. Rendering is optional via GPU backends.

## Current Honest Status

### ✅ Fully Functional (tested, works in practice)
- **Math** — Vec2/3/4, Mat4, Quat, radiometry, BRDF, GGX, SSS, volumetric fog
- **ECS** — sparse sets, type-safe components, generation-based validation
- **Memory** — Linear, Stack, Pool allocators
- **Events** — subscribe/publish/process queue, deferred dispatch
- **Thread Pool** — submit/wait_all/parallel_for, work-stealing
- **Asset Loading** — sync/async with reference counting
- **Scene Graph** — hierarchical transforms, dirty flag propagation
- **Serialization** — JSON/binary for save/load
- **Physics** — Semi-implicit Euler, impulse resolution, friction, restitution

### ❌ Stubbed (API exists, returns nullptr/empty — will fail at runtime)
- **GPU Rendering** — VulkanDevice/D3D12Device print message, return nullptr for buffers/textures
- **Audio Playback** — AudioEngine::init() returns true, no actual audio output
- **Input Polling** — InputManager::update() does nothing, key_pressed() always returns false
- **Scripting** — Lua/Python/C# engines create instances but execute nothing
- **Editor** — no UI, no gizmos, no asset browser

### ⚠️ Partially Implemented
- **World Generation** — Python API works, but procedural generation produces no geometry
- **Networking** — packet structs exist, no actual UDP/TCP
- **Advanced Rendering** — shaders exist on disk, no GPU dispatch

## What Needs To Be Done (Priority Order)

### Phase 1: Core Runtime (Required for any playable game)

1. **GPU Rendering Backend**
   - Add SDL2 + GLEW via vcpkg
   - Implement `OpenGLDevice` (replaces NullGPUDevice)
   - Must support: create_buffer, create_texture, create_shader, begin/end_frame, present
   - Reference: `alt/src/native/littcore/litt_gpu.h` (interface), `alt/src/native/littcore/litt_gpu.cpp` (stub)

2. **Input Backend**
   - Add SDL2 for keyboard/mouse/gamepad
   - Implement real polling in `InputManager::update()`
   - Must support: key_down, key_pressed, mouse_position, gamepad axes/buttons
   - Reference: `alt/src/native/littcore/litt_input.h` (interface)

3. **Window Management**
   - SDL2 window creation and event loop
   - Integrate with Engine::run() (currently does nothing)

### Phase 2: Audio & Physics

4. **Audio Backend**
   - Add miniaudio (single-header, public domain)
   - Implement `MiniaudioBackend` (replaces stub AudioEngine)
   - Must support: load_clip, play, stop, set_volume, 3D positioning
   - Reference: `alt/src/native/littcore/litt_audio.h` (interface)

5. **Physics Integration**
   - Implement semi-implicit Euler in `PhysicsEngine::update()`
   - Add force accumulation, velocity integration, position update
   - Implement collision response (impulse application, friction, restitution)
   - Reference: `alt/src/native/littcore/litt_physics.h` (interface)

### Phase 3: Scripting & Editor

6. **Lua Scripting**
   - Add Lua 5.4 + sol2 (header-only)
   - Implement real script loading, function calling, C++ binding registration
   - Reference: `alt/src/native/littcore/litt_scripting.h` (interface)

7. **Python Scripting**
   - Add pybind11
   - Implement Python module with engine API bindings
   - Reference: `alt/src/native/littcore/litt_scripting.h` (interface)

8. **C# Scripting**
   - Add Mono runtime
   - Implement assembly loading, MonoBehaviour lifecycle
   - Reference: `alt/src/native/littcore/litt_scripting.h` (interface)

9. **Editor UI**
   - Add ImGui
   - Implement viewport, hierarchy, inspector, gizmos
   - Reference: `alt/src/native/littcore/litt_ui.h`, `litt_ui_system.h` (interfaces)

### Phase 4: Advanced Features

10. **Networking**
    - Add enet or similar
    - Implement client-server, state sync, lag compensation
    - Reference: `alt/src/native/littcore/litt_networking.h` (interface)

11. **World Generation**
    - Implement actual geometry generation (heightmaps, biomes)
    - Output to Mesh/Material/ECS entities
    - Reference: `alt/tools/template/worldgen/` (generators exist but produce no geometry)

## Implementation Rules

1. **Read the interface first** — every subsystem has a header in `alt/src/native/littcore/litt_*.h`. Read it before implementing.
2. **Replace the stub** — find the stub implementation (grep for `// stub` or `return nullptr`), replace with real logic.
3. **Update the factory** — `create_gpu_device()` in `litt_gpu.cpp` should return your real device instead of `NullGPUDevice`.
4. **Test it** — add a test to `litt_engine_tests.cpp` that verifies the real implementation works.
5. **Mark it done** — update `alt/docs/Engine_Steps_Overview.md` from `[~]` to `[x]` with the real file path.

## Dependencies to Install

```bash
vcpkg install sdl2 glew miniaudio lua pybind11 imgui sol2
```

Or download single-headers:
- miniaudio: https://github.com/mackron/miniaudio (single header, public domain)
- sol2: https://github.com/ThePhD/sol2 (header-only, MIT)
- pybind11: https://github.com/pybind/pybind11 (header-only, BSD)

## File Locations

| What | Where |
|------|-------|
| Engine interfaces | `alt/src/native/littcore/litt_*.h` |
| Stub implementations | `alt/src/native/littcore/litt_*.cpp` |
| Tests | `alt/src/native/littcore/litt_engine_tests.cpp` |
| Build config | `vcpkg.json`, `CMakeLists.txt` |
| Checklist | `alt/docs/Engine_Steps_Overview.md` |
| Implementation plan | `alt/docs/IMPLEMENTATION_PLAN.md` |

## Directory & Path Rules (MANDATORY)

1. **NEVER create new folders** unless explicitly asked. Always work INSIDE the existing project directory.
2. **NEVER guess paths.** The project is at `D:\Allgemein\AI Router\litt engine\` (engine) or `D:\Allgemein\AI Router\litt spill\` (games). Use these EXACT paths.
3. **ALWAYS verify paths** with `ls` before using them. If a path doesn't exist, STOP and ask — don't guess.
4. **NEVER work outside the project.** Don't clone to random locations, don't create temp folders elsewhere, don't redirect output to unexpected places.
5. **ALL changes go to the existing repo.** Git commits, pushes, file edits — everything happens in `D:\Allgemein\AI Router\litt engine\`.
6. **Game projects live in `D:\Allgemein\AI Router\litt spill\<game_name>\`. NEVER create `D:\Allgege\...` or similar typo-paths.

## Routing (Updated)

- "implement GPU/audio/input" → Read `alt/docs/IMPLEMENTATION_PLAN.md`, pick a subsystem, read the interface, implement the backend, test it
- "make a game" → Use what works (ECS, events, math, worldgen Python API). Stubbed subsystems will fail at runtime — implement them first if you need rendering/audio/input. **Game projects go in `D:\Allgemein\AI Router\litt spill\<game_name>\`. NEVER create a new folder path.**
- "fix a bug" → Read the interface, find the stub, implement the real logic, add a test
- "add a feature" → Check `alt/docs/Engine_Steps_Overview.md` for `[~]` or `[ ]` items
- "make a game" (full workflow) → Create `D:\Allgemein\AI Router\litt spill\<game_name>\` ONCE. Inside: `config/world_state.json`, `assets/models/`, `assets/scenes/`, `engine/`. Use `litt_game.h` Game class. Build with `g++ -I "D:\Allgemein\AI Router\litt engine\alt\src\native\littcore"`.

## The Tool-Usage Law (MANDATORY)

Before claiming something is missing, broken, or non-existent, you MUST prove it:
1. **Glob** — discover what files actually exist
2. **Read** — inspect actual file content
3. **Run** — compile and execute tests, observe real output
4. **Report** — only then can you state what is present or absent

If you skip steps 1-3 and then report a gap, you are guessing. The human will know.

## Orient Before Write

1. Read `alt/docs/Engine_Steps_Overview.md` for current status
2. Read `alt/docs/IMPLEMENTATION_PLAN.md` for dependency info
3. Read the interface header (`litt_*.h`) for the subsystem you're implementing
4. Read the stub implementation (`litt_*.cpp`) to find what needs replacing
5. THEN implement, test, and update the checklist

## Build Commands (Correct Paths)

```bash
# Engine tests
cd "D:\Allgemein\AI Router\litt engine"
g++ -std=c++17 -I alt/src/native/littcore -o tests.exe alt/src/native/littcore/litt_engine_tests.cpp alt/src/native/littcore/litt_math.cpp -lgdi32 -luser32 -lwinmm

# Game project
cd "D:\Allgemein\AI Router\litt spill\<game_name>"
g++ -std=c++17 -I "D:\Allgemein\AI Router\litt engine\alt\src\native\littcore" -o game.exe engine/game.cpp "D:\Allgemein\AI Router\litt engine\alt\src\native\littcore\litt_obj.c" "D:\Allgemein\AI Router\litt engine\alt\src\native\littcore\litt_json.c" "D:\Allgemein\AI Router\litt engine\alt\src\native\littcore\litt_world.c" -lgdi32 -luser32 -lwinmm
```

**NEVER** create `D:\Allgege\...` or similar typo-paths. If a path doesn't exist, STOP and ask.

Same seed = same bytes. World state written LAST. Asset index registered. Attribution recorded.

Full protocol: `alt/docs/hermes/AGENTS.md` | Algorithms: `alt/tools/template/worldgen/algokit.py` | Math: `alt/docs/procedural_asset_math.md`
