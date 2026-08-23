---

## 2026-08-23 01:46 - ACTION by ox-alpha (AI)
- prompt: Create a perpetual landscape with endless grass.
- action: perpetual grass landscape -> radius 2 (seed 1337)
- generated this run: 25 chunk(s) chunk_-2_-2.obj, chunk_-2_-1.obj, chunk_-2_0.obj, chunk_-2_1.obj, chunk_-2_2.obj, chunk_-1_-2.obj ...
- state: world_state.json + scenes/world.lscn.json rewritten; viewers may reload


---

## 2026-XX-XX — live-viewer standalone program added (by harness agent, human-requested)

- Human prompt: "make a live.exe inside Project/live/viewer, separate program, moddable for GUI makers; linux/android versions too; no litt.exe in releases"
- Files written (all inside Project/live/viewer/):
  - Cargo.toml (standalone, empty [workspace] to detach from engine, ZERO dependencies)
  - src/main.rs (~260 lines, std-only read-only HTTP server; GET-only contract enforced, 405 on writes)
  - README.md (usage + endpoints)
  - GUI_INSTRUCTIONS.md (special instructions for GUI developers; observation-only contract, data files, extension points)
  - build-all.ps1 / build-all.sh (windows / linux via zigbuild-or-WSL / android cdylib via NDK)
  - live.exe (337 KB, built + smoke-tested: viewer 200, api/state 200, api/log 200, traversal blocked, POST 405)
- World files untouched: world_state.json, chunks, assets unchanged (no world mutation).
- Engine files untouched. Nothing committed, nothing released.


---

## live-viewer universal build scripts (human-requested)

- Human prompt: viewer program for linux/android/windows + one script that adapts to the AI's environment
- Files: build-all.sh rewritten as UNIVERSAL detector (windows-gitbash/linux/macos/android-termux via TERMUX_VERSION/uname; mingw-missing -> msvc fallback; artifact resolution across pinned-target layouts); build-all.ps1 rewritten (same fallback + Resolve-BuiltBinary, linux zigbuild/WSL, android NDK .so)
- README building section replaced with per-platform one-command table
- Verified ON WINDOWS: ps1 -> [ok] live.exe 336KB; git-bash -> detected MINGW64, [ok] ./live.exe. Linux/mac/Termux paths untested here (no such host available).
---

## 2026-08-23 - REPAIR by ox-alpha (AI)
- trigger: fresh AI session found world_state.json referencing 25 chunks that were
  absent from its clone; misdiagnosed as history-scrub casualty.
- root cause: .gitignore line *.obj silently excluded EVERY game mesh (live chunks,
  demo houses, example-village) from all commits; repo shipped zero .obj files.
- fix: removed blanket *.obj (Rust build junk lives in /target/), unignored
  template/agent/actions.log, committed all mesh assets. State<->disk now consistent.
