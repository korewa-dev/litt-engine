# Litt Engine

Litt is a low-resource, headless-first game engine and procedural game-building toolkit. The project aims for a small runtime and dependency footprint while retaining useful game, tooling, and world-generation capability.

> **Release contract:** the primary tested path is the native generated-game runtime in `alt/src/native` together with the WorldGen/template tools. Other APIs in the repository may be partial, experimental, or legacy. See `alt/docs/SUPPORTED_RUNTIME.md`.

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
| C bridge entity/component/transform contract | **Supported/tested** |
| WorldKit helpers | **Supported/tested**, broader output quality audit ongoing |
| C++ Engine facade | Partial |
| Scene graph | Partial; persistence incomplete |
| Software/native preview renderer | Partial |
| Vulkan/DX12/OpenGL/Metal generic facade | Experimental/unavailable in current generic backend |
| Physics | Experimental |
| Audio | Partial |
| Scripting VM | Partial, bounded execution contract |
| C ABI / SDK | Supported, versioned and externally consumer-tested |
| Python/C#/Lua integrations | Experimental, not release-supported |
| Networking | Minimal TCP transport supported/tested; higher-level multiplayer experimental |
| Legacy FFI world deployment | Incomplete |
| `litt_engine2.h` | Legacy |
| Editor tooling | Demo/example; Litt GUI is a separate integration project |

## Verified build and smoke-test path

Linux:

```sh
make -C alt/src/native test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/test_worldkit.py
python3 alt/tools/template/tools/worldgen/test_gen_props.py
```

The `Stabilization` GitHub Actions workflow is the cross-platform release gate. It additionally checks ASan/UBSan, Windows native compilation/tests, standalone public-header compilation, and the C bridge contract.

Do not enable or depend on an experimental backend merely because an interface or build option exists. Promote features to supported status only after their behavior is covered by the release matrix.

## Runtime/API entry point

For new generated games and automated game-building work, start with the native generated-game runtime and `GAME_BUILD_PROTOCOL.md`. For the exact support boundary, read `alt/docs/SUPPORTED_RUNTIME.md`.

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

## Quick start

The supported path does not require vcpkg, Vulkan, an editor, or language bindings.

```sh
git clone https://github.com/korewa-dev/litt-engine.git
cd litt-engine
make -C alt/src/native test
make -C alt/src/native bin/littcli bin/littview
alt/src/native/bin/littcli validate alt/Project/example-village --frames 60
python3 alt/tools/template/tools/worldgen/test_worldkit.py
python3 alt/tools/template/tools/worldgen/worldgen_quality_gate.py
```

For Windows, use the commands encoded in `.github/workflows/stabilization.yml` as the authoritative clean-checkout build contract. The workflow compiles and runs native tests and verifies every public `littcore/*.h` header independently.

## Documentation

Start with:

- `alt/docs/SUPPORTED_RUNTIME.md` for the exact support boundary
- `alt/docs/reports/RELEASE_READINESS_ROADMAP.md` for release gates
- `alt/docs/reports/RESOURCE_BASELINE.md` for low-resource budgets
- `alt/docs/reports/WORLDGEN_QUALITY_RESOURCE_AUDIT.md` for measured generator results
- `alt/tools/template/tools/worldgen/README.md` for procedural generation usage

Other historical documents can describe experimental or legacy systems. Their presence does not promote those systems into the release-supported contract.

## License

MIT License. See `LICENSE`.


Scripting and language binding support is defined in `alt/docs/SCRIPTING_FFI.md`.
