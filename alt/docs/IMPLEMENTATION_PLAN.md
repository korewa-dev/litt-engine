# Litt Engine Implementation Plan

## Priority Order

### Phase 1: Core Runtime (Required for any game to work)
1. **GPU Rendering** - OpenGL 4.6 backend (cross-platform)
2. **Input** - SDL2 for keyboard/mouse/gamepad
3. **Window Management** - SDL2 for window creation

### Phase 2: Audio & Physics
4. **Audio** - miniaudio for 3D spatial audio
5. **Physics** - Semi-implicit Euler integration, collision response

### Phase 3: Scripting & Editor
6. **Lua Scripting** - sol2 for Lua bindings
7. **Python Scripting** - pybind11 for Python bindings
8. **Editor** - ImGui for in-game editor

### Phase 4: Advanced Features
9. **C# Scripting** - Mono runtime integration
10. **Networking** - UDP/TCP with enet or similar

## External Dependencies

| Library | Purpose | License | Size |
|---------|---------|---------|------|
| SDL2 | Window, Input, Events | zlib | ~1MB |
| GLEW | OpenGL extension loading | MIT | ~500KB |
| miniaudio | Audio playback | Public Domain | Single header |
| Lua 5.4 | Scripting | MIT | ~500KB |
| pybind11 | Python bindings | BSD | Header-only |
| ImGui | Editor UI | MIT | ~2MB |
| sol2 | Lua bindings | MIT | Header-only |

## Implementation Strategy

Each subsystem will have:
1. **Interface** (already exists in `litt_*.h`)
2. **Real Implementation** (new file `litt_*_impl.h/.cpp`)
3. **Factory Function** in `create_gpu_device()` / `create_audio_engine()` etc.
4. **Tests** that verify real functionality

## Current Status

- ✅ Math, ECS, Memory, Events - Fully functional
- ✅ Thread Pool - Fully functional
- ✅ Asset Loading (sync) - Functional
- ✅ Scene Graph - Functional
- ❌ GPU Rendering - Stub (returns nullptr)
- ❌ Audio - Stub (no output)
- ❌ Input - Stub (no polling)
- ❌ Physics Integration - Stub (no force integration)
- ❌ Scripting - Stub (no execution)
- ❌ Editor - Stub (no UI)

## Next Steps

1. Add SDL2 + GLEW via vcpkg
2. Implement OpenGL device (replaces NullGPUDevice)
3. Implement SDL2 input (replaces stub InputManager)
4. Implement miniaudio backend (replaces stub AudioEngine)
5. Implement physics force integration
6. Add Lua scripting with sol2
7. Add Python scripting with pybind11
8. Add ImGui editor
