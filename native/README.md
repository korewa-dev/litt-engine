# Litt Native Core (`native/`) - staged C/C++ rewrite

Directive: move the engine to C/C++, keeping Rust only where it earns its
place. This is a **staged migration**, not a big-bang port - every stage must
compile clean, pass its own tests, and leave the full battery green.

## Stage 1 (SHIPPED) - littcore C library + headless CLI

| file | role | ported from |
|---|---|---|
| `littcore/litt_json.[ch]` | dependency-free JSON scanner | `src/gameplay.rs` Json |
| `littcore/litt_obj.[ch]` | group-aware OBJ loader (named part meshes, per-group index remap) | `crates/asset/src/model.rs` ObjLoader |
| `littcore/litt_world.[ch]` | world_state config parse + contract gameplay sim (modes, physics defaults, solids from model bounds, tag entities, tiers+lunge, lives/score/goal) | `src/gameplay.rs` |
| `littcli.c` | headless project validator (`littcli validate <dir> [--frames N]`) | `play_native.py --dummy` path |
| `tests.c` | unit tests (21 checks) | gameplay.rs tests subset |

Build: `native\build.bat` (Windows, gcc/llvm-mingw) or `make -C native`
(POSIX). Tests: `native\build.bat test`.

Integration: `make_game.py` prefers `native/bin/littcli(.exe)` for step-4
validation and falls back to Python when the binary is absent.

## Stage 2 (SHIPPED, first light) - C++ renderer front-end

`littview.cpp` - single-file C++17 front-end consuming littcore:

- loads a game's world_state + lscn + every `model:` OBJ (cached per name)
- ports the engine bake: sun diffuse |n.l| + hemispheric ambient + centroid
  distance haze, tag-driven tints (enemy red / pickup gold / goal green)
- orbit camera with robust framing: fit radius from the 88th-percentile
  triangle-centroid distance (backdrop ground discs can't zoom us out),
  constant slant range, elevation auto-steepens to 55 deg for flat worlds,
  yaw auto-picks perpendicular to the long axis
- depth-buffered software rasterizer, gamma-correct output; BMP writer for
  offscreen verification (`render <dir> --out f.bmp`), Win32 DIB window
  mode (`window <dir>`, arrows-free slow auto-orbit), pixel selftest
- NOTE: studio.rs perspective uses f=1/tan(fov) => ~143 deg effective hfov;
  fine in its side panel, fisheye fullscreen - littview derives vfov from a
  proper 60 deg horizontal fov instead

Verified: selftest ok; six shipped games render with real content
(drowned-vow 66%, kingsfall 79%, reef-rest 67% frame fill, 22-42 distinct
colors each); LITT_DEBUG=1 prints rasterizer diagnostics.

## Stage 3 (LATER) - parity & retirement

As C++ reaches feature parity per subsystem, retire the matching Rust binary
paths. Rust stays where it is genuinely needed until replaced (today: the
Studio app shell, Vulkan backend plumbing, path tracer reference).

## Rules

- No third-party deps in littcore (libc only).
- Every port ships with unit tests proving behavioral parity on the contract
  level (mode resolution, physics constants, tier pacing, OBJ grouping).
- The Python AI toolchain keeps calling the same pipeline; validators may
  swap underneath transparently.
