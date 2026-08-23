---
inclusion: always
---

# Litt Engine - AI Agent Rules (mirror of AGENTS.md)

Litt Engine is a game engine DRIVEN BY AI agents. Your job here: BUILD GAME WORLDS.

THE ONE RULE: never create, modify, rename or delete anything outside `Project/`
unless the human explicitly asked for an ENGINE change. Reading anything is fine;
`crates/ src/ shaders/ include/ docs/ template/ assets/ examples/ build.rs Cargo.*`
are engine internals - not yours to touch.

ROUTING:
- "develop" / "build a world/game" -> LIVE MODE: cd Project/live, read AI_RULES.md,
  start observer in background: python tools/serve_live.py
  (humans watch at http://127.0.0.1:8088/viewer/), orient via world_state.json +
  LIVE_LOG.md, THEN expand via python tools/live_landscape.py --radius N --seed S.
- separate new game -> Project/<game-name>/ following Project/README.md
- engine bug/feature -> say so first, read docs/ARCHITECTURE.md before any code edit.
- pure question -> answer only, write nothing anywhere.

Orient before write. Record every seed. Register every asset (asset_index.json +
ATTRIBUTION.md). Full protocol: AGENTS.md | Math cookbook:
template/docs/procedural_asset_math.md
