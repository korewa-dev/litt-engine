# Litt Engine

Litt is a low-resource, headless-first game engine and procedural game-building toolkit. The release surface is intentionally smaller than the repository's historical research surface: everything in the supported surface must have executable behavior and CI evidence.

> **Authoritative contract:** `alt/docs/SUPPORTED_RUNTIME.md`.

## What can be used to make a game

The current release surface is green for:

| Area | Release status |
|---|---|
| Generated-game runtime and WorldGen | **GREEN - supported/tested** |
| C++ `litt.h` SDK | **GREEN - packaged and external-consumer tested** |
| C ABI / SDK | **GREEN - versioned and external-consumer tested** |
| Engine lifecycle / scene graph / persistence | **GREEN - bounded contract** |
| Software renderer / render pipeline / textures | **GREEN - bounded CPU contract** |
| UI retained-mode draw commands | **GREEN - tested** |
| Terrain / particles / culling / LOD | **GREEN - tested** |
| Lighting / PBR helpers / shadow state | **GREEN - tested** |
| Assets / materials | **GREEN - bounded OBJ/TGA/PBR core** |
| Physics | **GREEN - bounded linear AABB rigid-body core** |
| Audio | **GREEN - WAV decode and software stereo mixer** |
| Scripting VM | **GREEN - bounded embedded language subset** |
| Networking | **GREEN - bounded framed TCP transport** |
| Serialization / profiler / input / events / memory | **GREEN - tested** |
| Portable editor core | **GREEN - scene mutation and undo/redo** |
| Dither3D CPU processing and Dither C ABI | **GREEN - tested** |
| Precompiled SPIR-V/DXIL artifact validation | **GREEN - tested** |

The release renderer is the software renderer. Vulkan, DirectX 12, OpenGL, Metal and GPU ray tracing are **not release backends**. Their public capability contract is fail-fast/unavailable, and CI verifies they cannot masquerade as supported hardware rendering.

Historical/research translation units for accelerated APIs may remain in the repository for future work, but they are not part of `litt.h`, the packaged C++ SDK runtime path, or the definition of a complete Litt release.

## Build and prove the engine

Linux:

```sh
make -C alt/src/native test
make engine-contract-test
make gameplay-feature-test
make release-hardening-test
make release-package-test
make -C alt/src/native cpp-sdk-test
make -C alt/src/native network-test
make -C alt/src/native gpu-contract-test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/worldgen_quality_gate.py --require-native
```

The `Stabilization` workflow is the authoritative cross-platform gate. It additionally covers GCC, Clang, Windows/MSVC, ASan/UBSan, ThreadSanitizer concurrency checks, standalone public-header compilation, malformed-input and persistence hardening, SDK packaging, generated games and cross-OS WorldGen determinism.

## Make a game

Generate one directly:

```sh
make game GAME=mygame DESC="small haunted forest action adventure"
make -C alt/src/native bin/littcli
alt/src/native/bin/littcli validate alt/Project/mygame --frames 60
```

Or build against the packaged C++ SDK:

```sh
make -C alt/src/native cpp-sdk
```

Then include `<litt/litt.h>` and link `liblittcore.a`.

## Design rules

- Keep the default runtime dependency-light.
- Prefer bounded contracts over broad unverified feature claims.
- Unsupported operations fail explicitly.
- Generated worlds must be deterministic for a fixed seed.
- Increase proof before increasing release surface.

## Documentation

- `alt/docs/SUPPORTED_RUNTIME.md` - exact supported boundary
- `alt/docs/IMPLEMENTATION_STATUS.md` - current source inventory
- `alt/docs/graphics-api/graphics-api-status.md` - accelerated API capability boundary
- `alt/docs/reports/RELEASE_READINESS_ROADMAP.md` - release engineering gates
- `GAME_BUILD_PROTOCOL.md` - generated-game workflow

## License

MIT License. See `LICENSE`.
