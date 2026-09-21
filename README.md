# Litt Engine

Litt is a low-resource, headless-first game engine and procedural game-building toolkit. The project aims for a small runtime and dependency footprint while retaining useful game, tooling, and world-generation capability.

> **Release contract:** see `alt/docs/SUPPORTED_RUNTIME.md` for the exact tested boundary. A subsystem name or experimental backend existing in the repository does not by itself imply release support.

## Philosophy

- **Little runtime cost:** avoid large fixed allocations and unnecessary default dependencies.
- **Useful capability:** prioritize features that produce playable games rather than feature-count claims.
- **Headless first:** game/runtime validation must work without a graphics window.
- **Deterministic generation:** seeded world generation should be reproducible and testable.
- **Honest failure:** unavailable functionality must report failure instead of returning fabricated or successful-looking results.

## Capability status

| Area | Status |
|---|---|
| Generated-game native runtime | **Supported/tested** |
| Native JSON/world/OBJ contracts | **Supported/tested** |
| C ABI / SDK | **Supported/tested**, versioned and externally consumer-tested |
| C++ Engine facade | **Supported bounded headless contract** |
| Scene graph / persistence v1 | **Supported bounded contract** |
| Assets / materials | **Supported bounded core**; shader compilation/GPU upload/async extensions not promoted |
| Physics | **Supported bounded AABB rigid-body core** |
| Audio | **Supported bounded decode/software-mix core**; physical device output is platform-specific |
| Scripting VM | **Supported bounded language/runtime contract** |
| Software/headless renderer | **Supported bounded CPU renderer** |
| Vulkan/DX12/OpenGL/Metal generic facade | Experimental/unavailable |
| GPU ray tracing | Experimental/unavailable |
| WorldKit helpers | **Supported/tested** |
| Networking | Minimal TCP transport **supported/tested**; higher-level multiplayer experimental |
| Python/C#/Lua integrations | Experimental, not release-supported |
| Legacy FFI world deployment | Incomplete |
| `litt_engine2.h` | Legacy |
| Editor tooling | Demo/example; Litt GUI is a separate integration project |

## Verified build and smoke-test path

Linux:

```sh
make -C alt/src/native test
make -C alt/src/native asset-test
make -C alt/src/native gpu-contract-test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/test_worldkit.py
python3 alt/tools/template/tools/worldgen/test_gen_props.py
python3 alt/tools/template/tools/worldgen/worldgen_quality_gate.py
```

The `Stabilization` GitHub Actions workflow is the cross-platform release gate. It additionally checks GCC/Clang, ASan/UBSan, Windows native compilation/runtime, standalone public-header compilation, scene/engine persistence, asset/material behavior, the C bridge, networking, software rendering, and cross-OS WorldGen determinism.

## Runtime/API entry point

For new generated games and automated game-building work, start with the native generated-game runtime and `GAME_BUILD_PROTOCOL.md`. For the exact support boundary and subsystem limits, read `alt/docs/SUPPORTED_RUNTIME.md`.

## Quick start

The supported path does not require vcpkg, Vulkan, DX12, OpenGL, Metal, an editor, or language bindings.

```sh
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine
make -C alt/src/native test
make -C alt/src/native asset-test
make -C alt/src/native gpu-contract-test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/worldgen_quality_gate.py
```

For Windows, `.github/workflows/stabilization.yml` is the authoritative clean-checkout verification contract.

## Documentation

Start with:

- `alt/docs/SUPPORTED_RUNTIME.md` for the exact support boundary
- `alt/docs/graphics-api/graphics-api-status.md` for accelerated graphics status
- `alt/docs/reports/RELEASE_READINESS_ROADMAP.md` for release gates
- `alt/docs/reports/RESOURCE_BASELINE.md` for low-resource budgets
- `alt/docs/reports/WORLDGEN_QUALITY_RESOURCE_AUDIT.md` for measured generator results
- `alt/tools/template/tools/worldgen/README.md` for procedural generation usage

Historical/design documents may describe experimental systems. Their presence does not promote those systems into the release-supported contract.

## License

MIT License. See `LICENSE` for details.
