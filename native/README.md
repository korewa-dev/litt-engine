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

## Stage 2 (NEXT) - C++ renderer front-end

C++ layer consuming littcore: window + swapchain via Vulkan, reusing the
shader knowledge in `crates/fidelityfx/src/shaders` (CAS/FSR are already GLSL
and portable). The Rust renderer crate becomes the reference implementation.

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
