# assets/

Runtime asset directory for Litt Engine games.

## Layout

| Path | Purpose |
|------|---------|
| `asset_index.json` | Machine-readable index of available assets (agents enumerate this instead of scanning) |
| `models/` | OBJ/glTF meshes loaded via `litt-asset` |
| `textures/` | PNG/JPG/KTX2 textures |
| `audio/` | WAV (hound) and MP3 (minimp3) files |
| `scenes/` | Scene JSON files (`*.lscn.json`, `litt_scene::serialization`) |
| `replays/` | Recorded sessions (`*.litr`, `litt::replay`) |

Subdirectories are created on demand by the loaders; only the index ships
checked in so agents always have a valid manifest to start from.

## Creating assets

- **How to create assets as an agent:** `../template/docs/ai_asset_creation.md`
- **Math recipes for every primitive/environment:** `../template/docs/procedural_asset_math.md`
- **Generator script:** `python ../template/tools/procedural_assets.py <house|cottage|tree|crate|terrain> --name X`
- Game content belongs under `Project/<game-name>/`, NOT here - see `Project/README.md`.
