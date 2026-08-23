# live-viewer — Litt Engine Live Observer (standalone)

A dependency-free replacement for `tools/serve_live.py`. Serves the
Project/live world viewer so humans can watch the AI build in real time.

**READ-ONLY BY DESIGN.** GET only. There is no endpoint through which a
connected client can modify anything — mutations belong to the AI working
on disk, never to viewers. This is a hard rule from [`../AI_RULES.md`](../AI_RULES.md).

## Quick start

```
live.exe                 # auto-discovers the live dir above it
live.exe --port 8088 --bind 0.0.0.0     # share on LAN (at your own risk)
live.exe --root D:\path\to\Project\live
```

Then open: http://127.0.0.1:8088/viewer/

## Endpoints

| Route | What |
|-------|------|
| `/viewer/`, `/` | The web viewer (`index.html`) |
| `/world_state.json` | Current world state (what the viewer polls) |
| `/assets/...` | Generated models/textures |
| `/api/state` | JSON passthrough of `world_state.json` (for GUIs) |
| `/api/index` | JSON passthrough of `assets/asset_index.json` |
| `/api/log?tail=N` | Last N lines of `LIVE_LOG.md` (default 50, max 500) |

Everything else that is not GET → **405**.

## Building — one command, any platform

Run the script that matches your shell; it **detects your environment**
(windows / linux / macos / android-termux), picks toolchain flags, builds,
and drops a ready binary in this folder:

| You are on | Run | Get |
|---|---|---|
| Windows (PowerShell) | `.\build-all.ps1` | `live.exe` |
| Windows (git-bash)   | `bash build-all.sh` | `live.exe` |
| Linux                | `./build-all.sh` | `live` |
| macOS                | `./build-all.sh` | `live` |
| Android (Termux)     | `pkg install rust` then `./build-all.sh` | `live-android` |

The scripts handle the engine-repo gotcha for you: when built inside this
repo on Windows, root `.cargo/config.toml` pins the mingw target — if the
mingw linker is missing they automatically fall back to MSVC.

Cross builds (optional): `.\build-all.ps1 linux` (zigbuild/WSL),
`.\build-all.ps1 android` (NDK .so). Termux users should just build
natively on-device instead — no NDK needed.

Manual equivalent, if you prefer:

```
cargo build --release
```

## Why no dependencies?

So you can read all of `src/main.rs` (~250 lines) in one sitting and change
anything without semver fear. See [GUI_INSTRUCTIONS.md](./GUI_INSTRUCTIONS.md)
before adding crates.

## Platform notes

- **Windows:** `live.exe` ships here.
- **Linux:** build natively on Linux (or WSL) via `build-all.sh`.
- **Android:** there is no exe on Android; the target is a `.so` loaded by an
  app shell — see `build-all.sh` section ANDROID.
