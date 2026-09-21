# Supported Runtime Contract

This document defines the release-supported Litt Engine path. Interfaces outside this contract may exist for experimentation or compatibility, but they are not evidence of production readiness.

## Supported path

The primary supported path is the native generated-game runtime under `alt/src/native`, the bounded native/C++ support contracts below, and the WorldGen/template toolchain under `alt/tools/template`.

Release CI gates the supported contracts on GCC, Clang, ASan/UBSan, and Windows/MSVC where applicable, plus generated-game and cross-OS WorldGen validation.

## Release-supported contracts

### Generated runtime and tooling

- native C runtime used by generated games
- generated-game validation/runtime path
- hardened JSON/world/OBJ contracts
- WorldKit deterministic generation helpers
- representative WorldGen output/resource budgets
- versioned C bridge entity/component/transform contract and external SDK consumer
- minimal framed TCP transport in `litt_networking.h`

### C++ Engine facade and scene persistence v1

The headless C++ Engine facade is release-supported for lifecycle, default-scene ownership, save/load, and scene persistence v1.

Scene persistence v1 supports scene identity, hierarchy, transforms, visibility, and cullable flags. It is transactional on load failure and enforces:

- serialized JSON: at most 8 MiB
- scene nodes: at most 65,536
- node names: at most 4 KiB

Runtime/render attachments are outside scene persistence v1 and are not implied by this contract.

### Asset and material core

The bounded asset/material contract supports:

- hardened native OBJ parsing used by the generated runtime
- C++ OBJ loading with finite-value and resource checks
- uncompressed 8/24/32-bit TGA loading with orientation normalization
- synchronous asset registry/import/reimport metadata flow
- PBR material factories and PBR-to-render material conversion

The C++ asset manager intentionally rejects its placeholder shader compiler. Asynchronous loading, GPU upload, shader compilation, and broad format support are outside this release contract.

### Physics core

The bounded physics contract supports up to 4,096 registered finite AABB rigid bodies with:

- sweep-and-prune broadphase
- AABB overlap contacts
- static bodies and trigger bodies
- linear force/impulse integration
- simple penetration and normal-velocity resolution
- explicit rejection/filtering of malformed and non-finite body state

Rotational dynamics, friction, joints, continuous collision detection, complex collider shapes, and a general-purpose production physics feature set are outside this contract.

### Audio core

The cross-platform audio core supports:

- bounded PCM WAV decoding
- source state semantics
- a dependency-free software mixer producing interleaved stereo float PCM
- mono/stereo source mixing
- per-source volume, pitch, looping, pause, play, and stop
- master volume and output clamping

The software mixer supports at most 256 registered sources and at most 1,048,576 output frames per render call. Cross-platform physical audio-device output is not part of this contract. Windows waveOut remains an optional platform integration.

### Scripting VM

The bounded scripting VM is release-supported through both C++ and the versioned C ABI for its documented compact language subset:

- variable declarations
- finite numeric, boolean, nil, and single-token string literals
- existing-variable reads
- `print` and `return`
- deterministic left-to-right arithmetic/comparison/logical binary expressions
- unary `not`
- configurable source-byte, instruction, and stack-value limits

Unsupported control-flow syntax such as `if`, `while`, and `function` fails at compile time instead of producing placeholder bytecode. Python, C#, and Lua are separate experimental integrations.

### Software renderer

The headless software renderer is release-supported for its bounded CPU rendering contract:

- CPU vertex/index buffers
- RGBA8 textures
- framebuffer clear and pixel access
- 2D filled triangles
- depth-tested rasterization
- clipped 3D triangles and indexed meshes
- grid/cube/terrain helper rendering
- 1000-frame headless soak

Allocations are bounded to 64 MiB per software buffer and 16,777,216 pixels per software image. Win32 window presentation exists as a platform-specific extension and is not a cross-platform presentation guarantee.

## Experimental or unavailable

- generic Vulkan/DX12/OpenGL/Metal renderer facade backends that return unavailable
- GPU ray tracing
- FFI world deployment stubs
- Python/C#/Lua integrations not exercised by the Stabilization matrix
- legacy `litt_engine2.h` JSON runtime

For Vulkan/DX12, the fail-fast contract is green: selection must reject or initialization must fail explicitly until a real backend is promoted. This is not accelerated rendering support.

## Compatibility rule

Unsupported or unavailable operations must fail explicitly. They must not return fabricated GPU data, fake entity IDs, mock framebuffers, successful no-op mutations, or placeholder shader success.

## Resource rule

The supported path remains small and dependency-light. New default dependencies, large fixed allocations, excessive generated assets, or substantial runtime memory increases require measurement and justification.

## Promotion rule

A partial or experimental subsystem becomes release-supported only after it has:

1. an explicit behavioral and error contract;
2. regression tests for success and failure cases;
3. Windows/Linux compile and runtime coverage where applicable;
4. sanitizer coverage for memory-unsafe code paths;
5. documentation matching the tested behavior;
6. successful validation on the exact merged `main` SHA.

## Platform prerequisites

The supported POSIX path needs a C11 compiler, a C++17 compiler, `make`, and Python 3 for WorldGen/tests. It does not require a graphics SDK or package manager. Windows release verification uses the native compiler environment provided by the CI runner.

The supported path does not require Vulkan, DX12, OpenGL, Metal, an editor, or language bindings.
