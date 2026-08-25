<!-- REMOVED STACK NOTICE (CDR-007): The Rust engine described here was removed from the repo; this document remains as design reference for the C/C++ port (native/littcore). -->
# AGENTS.md — AI Agent Entry Point. Read BEFORE writing anything.

You are looking at **Litt Engine**: an ultra-lightweight C/C++ game engine
with Python worldgen tooling, designed to be *driven by an AI*. Your job here
is almost always to BUILD A GAME WORLD, not to modify the engine.

## The One Rule

> You are the builder of games, NOT the modifier of the engine.
> Unless the human explicitly asks you to change the engine itself, you must
> not create, modify, rename, "fix", reformat or delete ANYTHING outside
> `Project/`.

Engine internals — do not touch without an explicit engine-task instruction:

```
native/  studio/  shaders/  include/  docs/  template/  assets/
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

# 2. open the native viewer so humans can watch your work live
litt view live                      # C++ orbit viewer (littview window)

# 3. orient: current world extent, recent actions, existing assets
read world_state.json               # chunks, seed, camera
read last 30 lines of LIVE_LOG.md   # what previous sessions did
read assets/asset_index.json
```

4. Only NOW build: expand via `python Project/live/tools/live_landscape.py --radius N --seed S ...`
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
complete folder (assets/index/scene/state). Copy it or study it.

**Runtime bridge:** the native contract lives in `native/littcore` (C) -
`litt_world.c` consumes any generated world as-is (movement mode from
state.identity, physics constants from state.gameplay.physics verbatim,
pickups/goals/hazards/enemies from node tags). `native/littcli validate`
proves it headlessly; `litt view <game>` shows it. The browser stack
(play.html/runtime.js) was removed - do not reintroduce HTML.

**Genre generators - run, do not rewrite:** `template/tools/worldgen/` ships
gen_soulslike.py, gen_space.py, gen_tabletop.py, gen_platformer25d.py,
live_landscape.py. For ANY named game type use **gen_archetype.py
--archetype <name>** (76 archetypes x 6 layout patterns x 26 themes, plus
--time-of-day/--weather). gen_custom.py is the raw scaffold for anything else.
Identity lookups: design_types.json + design_rules.json there; themes:
themes.json; the math per genre: `template/docs/genre_algorithms.md`.

**ONE-COMMAND FULL GAME — prefer this over hand-running the pipeline:**

```bash
python template/tools/worldgen/make_game.py --random
# or directed by any human phrase ("a game about haunted malls"):
python template/tools/worldgen/make_game.py --about "zombie mall survival"
```

make_game.py picks archetype/pattern/theme/kit, auto-authors brief.json,
runs generate -> props -> enrich -> lint (template/tools/assets/lint.py)
-> native validation, deploys ENGINE.bat/.sh + VIEW.bat + VALIDATE.bat + NOTES.md +
ATTRIBUTION.md, and registers the game in `Project/games.json`. Its last
stdout line is machine-readable JSON. Override anything with --name,
--seed, --archetype, --pattern, --theme, --kit.

**ONE-PHRASE MULTI-REGION WORLD — WorldForge (CDR-011):**

```bash
litt forge "any phrase"
# -> multi-region fused world via litt.worldforge/1 spec
```

world_planner.py decomposes the phrase into an explicit, hand-editable
litt.worldforge/1 spec (2-5 regions: generator/archetype/pattern/theme/role/
links); world_forge.py fuses them into ONE playable game (namespaced assets,
portal links on region boundaries, spawn + objective chain, lint + littcli +
native proof gates). Re-roll / fail-forward loop: `litt refine --kind <kind>
--base-seed <seed>`.

**STUDIO WINDOW — `litt studio [game]`:** real native window (Vulkan) with a
chat panel on the left and a live orbiting viewport of the loaded world.
Chat commands run the same tools in background jobs and hot-reload the view:
`make random`, `make about <text>`, `load <name>`, `regen`, `help`. Use it to
demo builds; use make_game.py directly when scripting.

**MANDATORY GAME PIPELINE — never ship a bare generated world.** A world made
only by a generator is geometry, not a game. Every new game under
`Project/<name>/` MUST run all four steps, in order:

```bash
python template/tools/worldgen/gen_<genre>.py --out-dir Project/<name> ...   # 1. geometry
python template/tools/worldgen/gen_props.py --game-dir Project/<name> \
    --kit survivor|platformer|souls                                           # 2. prop kit (model: refs)
# 3. author Project/<name>/brief.json (objective, side_objectives, physics,
#    enemy_aggro_m, corpse_run, scoring, waves, roster, spawn, checkpoints,
#    nodes[], zones[]) then:
python template/tools/worldgen/enrich_game.py --game-dir Project/<name> \
    --brief Project/<name>/brief.json --seed <S>                              # 3. gameplay layer
python Project/<name>/play_native.py --project Project/<name> \
    --frames 30 --dummy                                                       # 4. native validation
```

Step 4 must print `interactives` > 0 and `solids` > 0 (platformers). Then run
`native/bin/littcli validate Project/<name> --frames 120` — the engine itself
must deploy your world with **zero missing models** before you may call it
done. Hand-rolled box rooms instead of this pipeline are a rules violation.

**AI-generated assets:** textures may come from Stable Diffusion via
`template/tools/assets/gen_texture.py` (A1111-compatible server, or labeled
procedural fallback) and images become terrain via `gen_heightfield.py`.
See `docs/ai_textures.md`. Provenance logging + index registration apply.

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
(see `docs/agent-entry-points.md`). All mirrors are CDR-007-clean: they
describe the C/C++ + Python-worldgen stack only.
