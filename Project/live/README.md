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
- directly on the local PC (native window),
- inside OpenCode, Claude, or similar environments,
- or inside a Litt Editor created by contributors.

**Watch it:** `litt view live` - the C++ orbit viewer (littview) renders
the world from `world_state.json` on disk as the AI modifies it.

The viewer has **no editing capability by design** - observation is the
only interaction.

## Live Server Mode (retired)

The old read-only HTTP observer (`tools/serve_live.py`, browser viewer)
was removed when the HTML stack was phased out. The same guarantee holds:
only the AI mutates; every watcher observes. A future native live-watch
mode in littview will re-offer remote observation without any browser.

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
| `assets/` | generated world content (own asset_index.json) |
| `scenes/world.lscn.json` | current placed world |
| `world_state.json` | machine-readable live state polled by viewers |
| `LIVE_LOG.md` | append-only log of every AI action |
