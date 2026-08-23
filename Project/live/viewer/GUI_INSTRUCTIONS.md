# GUI_INSTRUCTIONS.md — Building a GUI for Litt Engine

So you want to turn this little observer into a real GUI for the engine.
Good news: the hard part is already decided for you. Read this once, fully.

## 0. The one rule you may not break

> **Viewers observe. Only the AI mutates.**

This comes from [`../AI_RULES.md`](../AI_RULES.md) and it is the core design
decision of live mode:

- Every endpoint in this server is **GET-only by contract**, not by omission.
- A GUI that can *write* world files is not an extension of Litt Engine —
  it is a violation of its operating model. Humans prompt; the AI acts.
- If you need interactivity (camera presets, filters, "request expansion"
  buttons), model it as **requests**: write a suggestion file OUT OF BAND or
  have the GUI talk to YOUR OWN backend which then asks a human/agent — never
  direct file writes served by this process.

If you fork this into something writable, rename it. It is then no longer
"live-viewer" and no longer bound by this file — but also no longer part of
the trusted observation path.

## 1. The data contract (stable, versioned)

Everything a GUI needs already exists as plain files under `Project/live/`:

| File | Role | Notes |
|------|------|-------|
| `world_state.json` | World extent, camera hints, chunk list | Poll it. Format field: `litt-live-state`, version 1. Written LAST by agents — never references missing files. |
| `assets/asset_index.json` | Machine-readable catalog of generated assets | Your menu/list source |
| `LIVE_LOG.md` | Append-only action history | Timestamped; use `/api/log?tail=N` |
| `viewer/index.html` | Reference implementation of a consumer | 181 lines; steal from it |

Rules for consumers:

- `world_state.json` uses cache-busting (`?ts=`) because servers send
  `no-store`. Do the same.
- Treat unknown fields as additive. New agent tooling WILL add fields.
- Never assume a file referenced in state exists mid-session — agents expand
  worlds while you watch; handle 404 gracefully and re-poll.

## 2. Architecture of this server (read main.rs, it's short)

- One thread per connection, std-only. No async runtime needed at this size.
- Request line parsed manually; routes are a match statement.
- `normalize()` blocks path traversal — keep that function if you change routing.
- Responses always set `Cache-Control: no-store`.

Extension points that keep the contract intact:

1. **New GET projections** (`/api/<thing>`): pure reads of existing files.
   Follow `serve_log_tail` as the template.
2. **Server-Sent Events / WebSocket push**: replace polling with push.
   Still read-only — you're just streaming observations faster.
3. **Embed a renderer** (wasm module, native viewport): serve static wasm,
   or spawn your own window process that consumes `/api/state`.
4. **GUI toolkit takeover** (egui/iced/slint/Tauri): drop the HTTP layer,
   keep `discover_root()` + the data contract, render directly from files.
   This binary intentionally has zero deps so that migration is a copy-paste,
   not a refactor.

## 3. Adding dependencies — the etiquette

The empty `[dependencies]` section is a message, not laziness:

- Every crate you add is a crate every future GUI dev must audit.
- Prefer: std first → tiny focused crates second → frameworks last.
- If you need JSON: `serde_json` is fine. If you need a window: pick ONE
  toolkit and isolate it behind your own trait so the core stays portable.
- Keep the release binary small where possible — the engine's identity is
  "< 1 MB"; a GUI bloating to 40 MB should be a conscious choice, not drift.

## 4. Engine hooks (future-facing)

The engine itself does not yet link into this program — deliberately.
When engine integration arrives (native camera control, live entity stream),
expect it to appear as either:

- a new read-only API surface published by the ENGINE side, which this
  server proxies, or
- a separate crate (`litt-gui-protocol` style) both sides compile against.

Design your GUI against the FILE contract today and both futures stay open.

## 5. Checklist before you call it done

- [ ] Still GET-only (or renamed out of the observation role entirely)
- [ ] `--help` works; auto-discovery of `world_state.json` still functions
- [ ] Path traversal guard untouched/equivalent
- [ ] Handles missing files mid-expansion without crashing
- [ ] Works with `--bind 127.0.0.1` by default; opt-in exposure only
- [ ] README updated with your endpoints

Welcome aboard. Keep the observer honest and build something great on top.
