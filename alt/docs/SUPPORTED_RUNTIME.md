# Supported Runtime Contract

This document is the authoritative release boundary for Litt Engine.

A feature is release-supported only when it has executable behavior, bounded failure semantics, cross-platform coverage where applicable, and a green Stabilization gate. Historical source files and research backends do not become supported merely because they are present in the repository.

## Primary game-building paths

Litt has two supported ways to build a game:

1. **Generated games** - `make_game.py` creates a complete project from a description or seeded generator configuration, then runs lint and native proof gates.
2. **C++ games** - include `<litt/litt.h>` from the packaged C++ SDK and link `liblittcore.a`.

The generated-game CI gate builds native tools, validates the checked-in fixture for 60 frames, generates a brand-new game from a natural-language description, runs that generator's own native proof, validates the fresh result for 60 frames, and compares representative WorldGen output across Linux and Windows.

## Release-supported runtime

### Core, scene and persistence

- C++17 math, ECS, events, bounded memory helpers, input and configuration
- scene hierarchy, transforms and lifecycle
- transactional scene persistence v1
- C++ Engine lifecycle with the software renderer as the default backend
- versioned C bridge for entity/component/transform use

Scene persistence v1 is bounded to 8 MiB serialized JSON, 65,536 nodes, and 4 KiB node names.

### Rendering and visual runtime

The portable software path is the release renderer.

Supported behavior includes:

- software GPU device creation through `create_gpu_device("software")`
- CPU buffers and RGBA8 GPU-abstraction textures
- software shader/uniform objects
- CPU render targets
- framebuffer clear, pixels, lines, rectangles and filled triangles
- depth-tested and clipped 3D triangles and indexed meshes
- generic `Renderer` facade backed by the software renderer
- CPU texture storage, PPM/TGA loading, mip generation, binding and atlases
- render-target/pipeline passes with geometry, lighting, post-processing and UI callbacks
- CPU tone mapping, simple bloom and simple FXAA post-processing
- stable light IDs, directional/point/spot direct PBR lighting and bounded shadow-map state
- environment diffuse/specular approximation helpers
- Dither3D generated patterns and CPU RGBA post-processing
- precompiled SPIR-V and DXIL artifact validation/loading

Software image allocation is capped at 16,777,216 pixels and software GPU buffers at 64 MiB.

### Gameplay-facing systems

The canonical `litt.h` surface includes and tests:

- AABB rigid-body physics with broadphase, contacts, triggers, forces and impulses
- deterministic terrain chunks and seeded noise generation
- frustum and conservative software occlusion culling
- bounded particle simulation and emitter shapes
- retained-mode UI interaction with deterministic draw-command output
- bounded LOD groups and registered LOD components
- JSON, binary and scene serialization utilities
- profiler timing and frame statistics
- scene/world simulation helpers
- deterministic input action bindings

Physics is a bounded linear AABB core, not a claim of joints, rotational rigid-body dynamics, CCD or arbitrary collider support.

### Audio

Supported audio behavior includes:

- bounded PCM WAV decoding
- source play/pause/stop state
- dependency-free software stereo mixing
- mono/stereo mixing, volume, pitch, looping and master gain

The mixer supports at most 256 sources and 1,048,576 output frames per render call. Cross-platform physical audio-device output is not claimed. Win32 waveOut remains an optional platform sink.

### Assets and materials

Supported behavior includes:

- hardened OBJ parsing
- uncompressed TGA asset loading
- synchronous asset registry/import/reimport and metadata
- PBR material factories and render-material conversion
- deterministic Dither assets

General image codecs, asynchronous GPU upload and runtime source shader compilation are not claimed. Shader source objects are portable runtime metadata; native GPU shader compilation is outside the software release path.

### Scripting

The embedded VM supports its documented bounded subset:

- variables and finite literals
- existing-variable reads
- arithmetic, comparison and logical expressions
- unary `not`
- `print` and `return`
- configurable source, instruction and stack limits

Unsupported control-flow syntax fails at compile time. Python, C# and Lua integrations are not part of the release runtime.

### Networking

The release contract includes the bounded framed TCP server/client transport in `litt_networking.h`, with Linux and Windows runtime coverage.

It does not claim a full replication, prediction, matchmaking or multiplayer gameplay framework.

### Editor core

The portable editor core supports:

- scene-backed selection and mutations
- create, rename, reparent and delete operations
- bounded snapshot undo/redo
- bounded editor/chat command history
- CLI integration using the same editor core

A full visual desktop GUI is a separate integration project and is not implied.

### SDKs

The release gate covers:

- the versioned C ABI/SDK consumer path
- the standalone Dither C ABI
- the packaged C++ SDK: canonical headers plus `liblittcore.a`
- a game-level C++ contract that includes `litt.h` and exercises gameplay/rendering systems together

## Explicitly unavailable

The following are not release features:

- generic Vulkan renderer backend
- generic DirectX 12 renderer backend
- generic OpenGL renderer backend
- generic Metal renderer backend
- generic Vulkan/DX12/OpenGL/Metal renderer facade backends that return unavailable
- GPU ray tracing / DXR / Vulkan RT
- named-vendor physical GPU certification
- Python/C#/Lua runtime integrations
- legacy `litt_engine2.h`

Selecting unavailable accelerated backends must fail explicitly. Source files retained for research or compatibility are not evidence that those backends work.

## Platform contract

The portable supported path requires:

- C11 compiler
- C++17 compiler
- `make`
- Python 3 for generated-game tooling and release tests

No graphics SDK or package manager is required for the supported path.

Win32 presentation and waveOut are platform extensions. Headless/software behavior is the cross-platform contract.

## Promotion rule

A new subsystem or backend becomes release-supported only after it has:

1. explicit success and failure behavior;
2. tests for both;
3. Windows/Linux coverage where applicable;
4. sanitizer coverage for memory-unsafe paths;
5. documentation matching the executable behavior;
6. successful validation on the exact merged `main` SHA.

The rule is simple: increase proof before increasing claims.
