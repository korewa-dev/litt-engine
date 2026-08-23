# Litt Engine - Live Development Mode

`Project/live` is the live deployment directory. When work happens here,
the AI is in **live-development mode**.

## The Contract

| Who | May | May NOT |
|-----|-----|---------|
| Human | prompt, observe, supervise | operate or modify the engine or game world |
| AI | generate, expand, maintain the world | delegate edits to humans |

Humans ask. Only the AI does.

Example instruction: *"Create a perpetual landscape with endless grass."*
The AI autonomously generates, expands, and maintains that world.

## Live Visualization

A visualization tool displays the AI's live work while it is being made.
It may appear:

- inside DeepSeek Harness,
- directly on the local PC (browser),
- inside OpenCode, Claude, or similar environments,
- or inside a Litt Editor created by contributors.

**Shipped here:** `viewer/index.html` - a read-only 3D observer that polls
`world_state.json` and renders the world as the AI modifies it.

Start it:

```bash
python tools/serve_live.py          # then open http://127.0.0.1:8088/viewer/
```

The window shows the live scene while the AI works. It has **no editing
capability by design** - observation is the only interaction.

## Live Server Mode (optional)

`tools/serve_live.py` doubles as the live server:

- other users may connect to view the live development,
- approved tools may display the scene,
- external viewers observe AI-driven changes in real time,
- access can be restricted (bind address / firewall / reverse proxy auth).

Server-level guarantee: only GET is served. No connected user can modify
anything; mutations physically have no endpoint. All modifications are
performed by the AI writing through its own tools.

## Tools & Editors

Third-party Litt Editors / visualization tools are welcome if they:

- show the live scene, AI actions, logs, metrics or debug info,
- run locally or remotely, connecting to the live server,
- **remain AI-exclusive**: display and telemetry only, no human editing.

Humans may observe, prompt, and supervise - never manipulate the game world.

## Files in this directory

| File | Purpose |
|------|---------|
| `AI_RULES.md` | binding protocol for any AI entering live mode |
| `tools/live_landscape.py` | chunked perpetual-world generator |
| `tools/serve_live.py` | read-only live server + visualization host |
| `viewer/index.html` | AI-exclusive live scene observer (browser) |
| `assets/` | generated world content (own asset_index.json) |
| `scenes/world.lscn.json` | current placed world |
| `world_state.json` | machine-readable live state polled by viewers |
| `LIVE_LOG.md` | append-only log of every AI action |
