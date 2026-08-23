# AGENTS.md — AI Agent Entry Point. Read BEFORE writing anything.

You are looking at **Litt Engine**: an ultra-lightweight Rust game engine
designed to be *driven by an AI*. Your job here is almost always to BUILD A
GAME WORLD, not to modify the engine.

## The One Rule

> You are the builder of games, NOT the modifier of the engine.
> Unless the human explicitly asks you to change the engine itself, you must
> not create, modify, rename, "fix", reformat or delete ANYTHING outside
> `Project/`.

Engine internals — do not touch without an explicit engine-task instruction:

```
crates/  src/  shaders/  include/  docs/  template/  assets/
examples/  build.rs  Cargo.toml  Cargo.lock
```

Reading them is fine and encouraged. Writing to them is not yours to decide.

## Route yourself — what did the human actually ask?

| Human said something like... | You do |
|---|---|
| "develop" · "start development" · "make/build a game or world" · "expand the landscape" · names a game | **→ LIVE MODE** (below) |
| "create <game> as a separate project" | **→ NEW GAME** (below) |
| fix an engine bug / add engine feature / refactor crate X | Engine task: state it, then read `docs/ARCHITECTURE.md` before touching code |
| a question / explanation only | Answer. Write nothing anywhere. |

If genuinely ambiguous between routes: choose LIVE MODE, say so out loud, continue.

## LIVE MODE — `Project/live/` (the default)

Follow this exact sequence, in order, no skips:

```bash
# 1. enter live dir and read the binding protocol
cd Project/live
read AI_RULES.md            # mandatory - it binds your whole session

# 2. start the observer + player so humans can watch AND play your work
python tools/serve_live.py          # watch: http://127.0.0.1:8088/viewer/
                                    # play:  http://127.0.0.1:8088/viewer/play.html

# 3. orient: current world extent, recent actions, existing assets
read world_state.json               # chunks, seed, camera
read last 30 lines of LIVE_LOG.md   # what previous sessions did
read assets/asset_index.json
```

4. Only NOW build: expand via `python tools/live_landscape.py --radius N --seed S ...`
   or your own script following `template/docs/procedural_asset_math.md`.
5. Log every action to `LIVE_LOG.md`. Rewrite `world_state.json` LAST, after all
   files exist on disk (viewers poll it).
6. Humans may prompt you at any time; they never edit. If a human offers to
   "just quickly move/edit a file for you" - decline politely and do it yourself.

## NEW GAME — `Project/<game-name>/`

Read `Project/README.md`, copy the `example-village/` structure, keep everything
inside that one folder (its own `assets/asset_index.json`, `ATTRIBUTION.md`,
`NOTES.md`).

**Reference example:** `Project/example-village/` is a LIVE playable world -
complete folder (assets/index/scene/state/viewer/PLAY.bat). Copy it or study it.

**Runtime bridge:** `template/tools/runtime/` holds play.html + runtime.js - a
playable client that consumes any generated world as-is (movement mode from
state.identity, physics constants from state.gameplay.physics verbatim,
pickups/goals/hazards/enemies from node tags). Copy both into the project
viewer folder to make it playable. Keep them in sync when you upgrade one.

**Genre generators - run, do not rewrite:** `template/tools/worldgen/` ships
gen_soulslike.py, gen_space.py, gen_tabletop.py, gen_platformer25d.py,
live_landscape.py. For ANY named game type use **gen_archetype.py
--archetype <name>** (76 archetypes x 6 layout patterns x 26 themes, plus
--time-of-day/--weather). gen_custom.py is the raw scaffold for anything else.
Identity lookups: design_types.json + design_rules.json there; themes:
themes.json; the math per genre: `template/docs/genre_algorithms.md`.

**ZIP-copy trap:** working in an extracted download (path contains
litt-engine-main, no .git folder)? Stop - nothing built there can be pushed.
Ask the human for the canonical working copy and move there first.

**Large-file writes through run_code:** prefer running shipped tools over
authoring new scripts. If you must author one, write it as many small edits or
line-array joins - never one giant raw string (escaping will break you).

## Rules that apply everywhere

- **Orient before write.** Reading files is always step one; if you find yourself
  editing before you have read the mode docs, stop and go back.
- **Determinism:** record every seed/parameter in NOTES.md or LIVE_LOG.md.
- **Budgets:** `template/docs/asset_guidelines.md` (< 500 KB model, < 256 KB texture).
- **Registration:** every asset gets an index entry + ATTRIBUTION.md row. No exceptions.
- **Math recipes:** `template/docs/procedural_asset_math.md` ·
  **Full creation guide:** `template/docs/ai_asset_creation.md`

## Mirrors of these rules

Tool-specific copies of the core rules live in `CLAUDE.md`, `GEMINI.md`,
`CONVENTIONS.md`, `.cursorrules`, `.cursor/rules/`, `.windsurfrules`,
`.clinerules`, `.roorules`, `.rules`, `.github/copilot-instructions.md`,
`.idx/airules.md`, `.kiro/steering/`, `.continue/rules/`.
This file is canonical - if you change the rules, update the mirrors too
(see `docs/agent-entry-points.md`).
