# Litt Engine — Game Build Protocol

**This is the ONLY official way to build games in the Litt Engine.**

## Quick Start

```bash
# One command to build a complete game
python alt/tools/template/tools/worldgen/make_game.py --about "your game description"
```

## Manual Pipeline (Advanced)

```bash
# Step 1: Generate geometry
python alt/tools/template/tools/worldgen/gen_<genre>.py --out-dir alt/Project/<game_name>

# Step 2: Generate props
python alt/tools/template/tools/worldgen/gen_props.py --game-dir alt/Project/<game_name> --kit survivor

# Step 3: Author brief.json, then run enrich_game.py
python alt/tools/template/tools/worldgen/enrich_game.py --game-dir alt/Project/<game_name>

# Step 4: Validate
python alt/tools/template/tools/assets/verify_project.py <game_name>
```

## Rules (MANDATORY)

1. **ALL games go in `alt/Project/<game_name>/`** — nowhere else
2. **Use `make_game.py`** — do NOT hand-write worldgen code
3. **Register in `games.json`** — `make_game.py` does this automatically
4. **Validate with `verify_project.py`** - run before declaring done
5. **Include all required files**: `NOTES.md`, `ATTRIBUTION.md`, `LIVE_LOG.md`, `assets/asset_index.json`, `ENGINE.bat`, `ENGINE.sh`, `VIEW.bat`, `VALIDATE.bat`

## Required Files Every Game Must Have

```
alt/Project/<game_name>/
├── world_state.json
├── assets/models/
├── assets/scenes/world.lscn.json
├── assets/asset_index.json
├── NOTES.md
├── ATTRIBUTION.md
├── LIVE_LOG.md
├── brief.json
├── ENGINE.bat
├── ENGINE.sh
├── VIEW.bat
└── VALIDATE.bat
```

## C++ Engine Integration (Optional)

```cpp
#include "litt_game.h"

int main() {
    litt::Game game;
    game.initialize("alt/Project/<game_name>");
    while (true) { game.update(1.0f/60.0f); game.render(1.0f/60.0f); }
}
```

Build: `g++ -std=c++17 -I alt/src/native/littcore -o game.exe game.cpp alt/src/native/littcore/litt_obj.c alt/src/native/littcore/litt_json.c alt/src/native/littcore/litt_world.c -lgdi32 -luser32 -lwinmm`

## See Also

- Worldgen tools: `alt/tools/template/tools/worldgen/`
- Existing games: `alt/Project/`
- Engine source: `alt/src/native/littcore/`
- Rules: `.continue/rules/litt-engine.md`

## Native proof

`verify_project.py` is the always-available structural validator. When `alt/src/native/bin/littcli` and `littview` have been built, `make_game.py` also runs the native simulation/render proof. Generated projects do not depend on the retired `play_native.py` launcher.
