# Litt Engine Implementation Inventory

> `SUPPORTED_RUNTIME.md` is authoritative for release support. This inventory describes the live canonical source surface after removal of the old blueprint mega-header.

## Canonical public C++ surface

The packaged C++ SDK is rooted at `litt.h`. `litt_engine_systems.h` is now an include umbrella only; it contains no duplicate fake subsystem implementations.

### Core

- `litt_math.h` - vectors, matrices, quaternions, AABB/OBB/rays
- `litt_ecs.h` - generational entities and component storage
- `litt_event.h` - dispatcher and thread-safe deferred queue
- `litt_memory.h`, `litt_memory_pool.h` - bounded memory helpers
- `litt_input.h` - deterministic input/action state
- `litt_config.h` - runtime configuration
- `litt_profiler.*` - timing and frame statistics

### Engine, scene and persistence

- `litt_engine.h` - lifecycle, default scene, update/render loop
- `litt_scene.h` - bounded hierarchy and transactional persistence v1
- `litt_serialization.h` - bounded JSON/binary/scene serialization
- `litt_world.h` / C world runtime - game-world state and generated-game simulation

### Rendering

- `litt_renderer.h` - software-backed renderer facade
- `litt_gpu.h`, `litt_gpu_software.h` - GPU abstraction and supported CPU implementation
- `litt_render_pass.*` - render targets and executable CPU pipeline passes
- `litt_texture.*` - bounded CPU texture storage, PPM/TGA loading, mipmaps and atlases
- `litt_lighting.*` - direct PBR lighting, environment helpers and bounded shadow-map state
- `litt_shader_system.*` - portable shader-source metadata/uniform/binding registry
- `litt_shader_artifacts.h`, `litt_shader_compilation.cpp` - precompiled SPIR-V/DXIL artifact validation
- `litt_dither.*` - generated Dither3D patterns and CPU RGBA post-processing

### Gameplay systems

- `litt_physics.h` - bounded linear AABB rigid-body physics
- `litt_audio.h` - PCM WAV state plus dependency-free stereo software mixer
- `litt_scripting_vm.h` - bounded embedded scripting subset
- `litt_networking.h` - bounded framed TCP client/server transport
- `litt_ui.h` - retained UI interaction and deterministic draw commands
- `litt_lod.h` - bounded LOD groups/components
- `litt_culling.*` - frustum and conservative software occlusion culling
- `litt_particle_system.*` - bounded deterministic particle simulation
- `litt_terrain_system.*` - deterministic chunk terrain and noise generation

### Assets and materials

- `litt_asset.h`, `litt_asset_pipeline.*` - bounded OBJ/TGA and synchronous asset registry/reimport
- `litt_material.h`, `litt_pbr_material.h` - material and PBR conversion helpers

### Tooling and SDKs

- `litteditor.h` - portable scene editor state with bounded undo/redo
- `litt_c.h` / `litt_c.cpp` - versioned C bridge
- `alt/include/litt_ffi.h` / `litt_ffi.cpp` - standalone Dither C ABI
- generated-game `littcli`, `littview`, WorldKit and WorldGen tools
- packaged C++ SDK with canonical headers and `liblittcore.a`

## Release evidence

The Stabilization matrix covers:

- GCC and Clang
- Windows/MSVC
- ASan/UBSan
- ThreadSanitizer concurrency contract
- public-header standalone compilation
- all-features gameplay contract
- generated-game end-to-end generation and validation
- C and C++ external SDK consumers
- networking malformed/partial-frame handling
- persistence properties and malformed-input smoke
- lifecycle and allocation-limit hardening
- deterministic release packaging
- cross-OS WorldGen determinism

## Retired or non-release research

These are not part of the live feature inventory:

- generic Vulkan / DirectX 12 / OpenGL / Metal renderer implementations
- GPU ray tracing / DXR / Vulkan RT
- vendor-specific physical GPU certification helpers
- Python / C# / Lua runtime integrations
- legacy `litt_engine2.h`
- the former duplicate blueprint systems previously embedded in `litt_engine_systems.h`

Their presence in historical/design files does not make them release features.
