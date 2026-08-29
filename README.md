# Litt Engine

> **NOTE:** This README accurately reflects current implementation status. See [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for details.

## What is Litt Engine?

AI-driven game engine designed for autonomous AI agents to build, control, and run fully-optimized games. The engine is headless-first with a C/C++ core, Python worldgen tooling, and a growing set of working games.

**Current state:** Header-heavy with partial implementations. Core math, OBJ loading, JSON parsing, and dither rendering are working. ECS, physics, renderer, audio, UI, and input are stubs awaiting implementation.

## Quick Links

| Resource | Link |
|----------|------|
| Main docs | [`docs/README.md`](docs/README.md) |
| Philosophy | [`docs/PHILOSOPHY.md`](docs/PHILOSOPHY.md) |
| Agent entry | [`docs/AGENT_ENTRY_POINTS.md`](docs/AGENT_ENTRY_POINTS.md) |
| Conventions | [`CONVENTIONS.md`](CONVENTIONS.md) |
| AI rules | [`Project/live/AI_RULES.md`](Project/live/AI_RULES.md) |
| Implementation status | [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md) |
| Genre algorithms | [`template/docs/genre_algorithms.md`](template/docs/genre_algorithms.md) |
| Asset guidelines | [`template/docs/asset_guidelines.md`](template/docs/asset_guidelines.md) |

## Project Layout

```
litt-engine/
├── native/                    # C/C++ engine core
│   ├── littcore/              # Core modules (headers + some implementations)
│   │   ├── litt_math.h/cpp    # ✅ Vec2/3/4, Mat4, Quat, AABB, raycasts
│   │   ├── litt_ecs.h         # ⚠️ Header-only: archetype ECS, no systems
│   │   ├── litt_physics.h     # ⚠️ Header-only: rigid bodies, no solver
│   │   ├── litt_renderer.h    # ⚠️ Header-only: no Vulkan/DX12 backend
│   │   ├── litt_audio.h       # ⚠️ Header-only: clip/source structs
│   │   ├── litt_ui.h          # ⚠️ Header-only: element declarations
│   │   ├── litt_input.h       # ⚠️ Header-only: key/mouse tracking
│   │   ├── litt_json.c        # ✅ JSON parser (working)
│   │   ├── litt_obj.c/cpp     # ✅ OBJ loader (working)
│   │   ├── litt_world.c/cpp   # ✅ World sim (working)
│   │   ├── litt_dither.cpp    # ✅ Dither3D PNG→3D texture (working)
│   │   └── litt_dither_vulkan.cpp  # ⚠️ Texture upload stub
│   ├── build.bat              # Windows build (llvm-mingw)
│   ├── tests.cpp              # ✅ Unit tests (23/23 pass)
│   ├── game.cpp               # Game entry point
│   └── littview.cpp           # Vulkan orbit viewer
│
├── template/                  # AI tooling & worldgen
│   ├── tools/worldgen/        # 21 generators, 76 archetypes × 6 patterns × 26 themes
│   └── docs/                  # Algorithm reference (executable)
│
├── Project/                   # Generated game projects (16 shipped)
│   ├── live/                  # Active development project
│   ├── four-districts/        # WorldForge demo
│   ├── forge-final-e2e/       # End-to-end validation
│   └── ...                    # (see ls Project/ for full list)
│
├── studio/                    # C# editor (Null Device LLC)
│   └── LittStudio.sln         # .NET Framework v4.8
│
├── tools/
│   └── litt.py                # CLI router (status/build/test/proof/new/forge/refine/play/view/bench/studio/doctor)
│
└── docs/
    ├── IMPLEMENTATION_STATUS.md  # ✅ Authoritative status tracker
    └── RENDERING/               # Graphics API docs
```

## CLI: `litt`

```bash
litt status          # Dashboard of projects, assets, tests
litt build [game]    # Build native binaries
litt test [game]     # Run worldgen + validate
litt proof [game]    # Headless proof run (120 frames, null backend)
litt new NAME        # Scaffold a new game project
litt forge "PHRASE"  # WorldForge: one-phrase multi-region world
litt refine --base-seed S --kind <kind>  # Iterative refine loop
litt play GAME       # Launch generated game
litt view GAME       # Orbit viewer (Vulkan)
litt bench [GAME]    # Performance benchmarks
litt studio [GAME]   # Editor (LittStudio.exe)
litt doctor          # Self-test
```

## Build (Native)

```bash
# Requires llvm-mingw (Windows cross-compile)
native/build.bat
# Outputs: native/bin/littcli.exe, native/bin/littview.exe, native/bin/game.exe, native/bin/dither3d_demo.exe
```

## One-Command Game

```bash
# Text prompt → full world
python template/tools/worldgen/make_game.py --about "a ruined temple overrun by vines"
python template/tools/worldgen/make_game.py --random

# WorldForge (multi-region fused world)
litt forge "cyberpunk city with underground markets"
```

## Worldgen Pipeline

```
Genre algorithm (terrain/rooms/paths)
    → gen_props.py (props + POIs)
    → enrich_game.py (brief + world_state)
    → littcli validate (missing model check)
    → play_native.py (120-frame headless proof)
```

See [`template/docs/genre_algorithms.md`](template/docs/genre_algorithms.md) for the full algorithm reference.

## Game Directory Layout

Every game under `Project/<name>/` has:

```
brief.json           # Human-readable design brief
world_state.json     # Canonical world state (seed, archetypes, layout)
asset_index.json     # All assets with provenance (source, license, SHA-256)
ATTRIBUTION.md       # Human-readable credit table
```

## Existing Games

```
ashen-oath  ash-reach  cinder-hold  crimson-fall  dither3d-demo
drowned-vow-42  ember-depths  example-village  forge-e2e  forge-final-e2e
four-districts  gull-point  kingsfall-hollow  live  reef-rest  skyline-run  worldforge-demo
```

Run any with `litt play <name>`.

## AI Rules (CDR-001..011)

See [`docs/AGENT_ENTRY_POINTS.md`](docs/AGENT_ENTRY_POINTS.md) for the full rule set. Key constraints:

- **Builder, not engine-modifier:** Agents build games inside `Project/`, don't touch engine internals unless explicitly tasked.
- **Integrity Law:** Each project has ONE coherent identity (archetype + pattern + theme + seed). Verify `world_state.json` and `brief.json` agree.
- **Tool-Usage Law:** Prove absence via `glob` → `read` → run → report. Anti-hallucination protocol requires re-verification.
- **No src/ directory** (Rust removed per CDR-007).
- **No zip-copy trap:** Working in extracted download? Stop; cannot push.

## Current Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Math (Vec2/3/4, Mat4, Quat, AABB, raycasts) | ✅ Working | `litt_math.cpp`, unit tests pass |
| JSON parser | ✅ Working | `litt_json.c` |
| OBJ loader | ✅ Working | `litt_obj.c` |
| World simulation | ✅ Working | `litt_world.c/.cpp` |
| Dither3D textures | ✅ Working | `litt_dither.cpp` |
| Vulkan dither demo | ✅ Working | `dither3d_demo.exe` |
| Unit tests | ✅ 23/23 pass | `tests.cpp` |
| ECS (entity/component storage) | ⚠️ Header-only | Archetype storage defined, no systems |
| Physics (rigid bodies) | ⚠️ Header-only | AABB broadphase declared, no solver |
| Renderer | ⚠️ Header-only | No Vulkan/DX12 backend implemented |
| Audio | ⚠️ Header-only | Clip/source structs, no backend |
| UI | ⚠️ Header-only | Element declarations, no framework |
| Input | ⚠️ Header-only | Key/mouse tracking, no platform binding |
| DX12 backend | ❌ Not implemented | Header declares types, no source |
| Shader compilation | ❌ Not implemented | No SPIR-V/DXIL compiler |
| Asset pipeline | ❌ Not implemented | No model/texture loading |

**For the authoritative status tracker, see [`docs/IMPLEMENTATION_STATUS.md`](docs/IMPLEMENTATION_STATUS.md).**

## Roadmap

1. **Fix README** — align with implementation reality ✅ (this file)
2. **Implement missing C core** — ECS systems, physics solver, renderer, audio, UI, input
3. **Complete DX12** — real DXR with acceleration structures
4. **Add shader compilation** — SPIR-V (Vulkan) and DXIL (DX12) pipelines
5. **Feasibility spikes** — MUSA (Moore Threads), NNAPI (Android NPU), Ryzen AI / Intel AI Boost
6. **Portable build system** — abstract llvm-mingw path, CMake support

## License

Copyright (c) 2024-2025, AI Router Team / Litt Engine contributors.