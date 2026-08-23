# Project Directory

**ALL game development output lives here.** This directory is the workspace
for games built WITH Litt Engine. The engine's own `assets/` folder stays
engine-only (examples, engine demo content) - never mix game content into it.

> **`live/` is special**: it is the live-deployment workspace where the AI works
> autonomously while humans observe and prompt only. See `live/README.md` and
> open the observer at `http://127.0.0.1:8088/viewer/` (`python live/tools/serve_live.py`).

## Layout - one folder per game

```
Project/
  <game-name>/              # kebab-case, self-contained
    assets/
      asset_index.json      # THIS game's manifest (agents read this first)
      models/               # .obj/.mtl or .glb
      textures/
      audio/
      scenes/               # *.lscn.json
    src/                    # game code (optional)
    NOTES.md                # agent log: decisions, seeds, TODOs
    ATTRIBUTION.md          # provenance for every asset in this game
```

## Rules

1. Every asset a game uses lives inside that game's folder. Self-contained =
   portable = another agent can pick up the game blind.
2. Register every asset in the **game's own** `assets/asset_index.json`
   (the generator script does this automatically when run with
   `--out-dir Project/<game-name>/assets`).
3. Same size budgets as the engine (`template/docs/asset_guidelines.md`),
   applied per game.
4. Asset creation method: follow `template/docs/ai_asset_creation.md`,
   math recipes in `template/docs/procedural_asset_math.md`. For generating an
   entire game by TYPE start at `template/docs/game_type_generation.md` -
   one generator command + copy the play runtime = finished playable world.
5. Record seeds and parameters in NOTES.md so any agent can regenerate.

## Quick start

```bash
mkdir -p Project/my-game/assets
cd Project/my-game
python ../../template/tools/procedural_assets.py house --name first_house --out-dir assets
python ../../template/tools/procedural_assets.py terrain --name ground --width 32 --out-dir assets
```

## Reference example

`example-village/` - a complete minimal game folder generated entirely by the
procedural pipeline: 8 assets, its own index, attribution, and a scene file
placing everything. Copy its structure, not just its files.
