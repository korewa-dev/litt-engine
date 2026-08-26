# Four Districts

A city exploration game built with the Litt Engine.

## Overview

Explore the metropolis of Oakhaven, divided into four distinct districts. Uncover the mystery behind the Great Fracture and unite the city.

## Districts

| District | Theme | Description |
|----------|-------|-------------|
| North | Financial Quarter | Skyscrapers, wealth, secrets in the ledgers |
| South | Industrial Zone | Smokestacks, forges, something darker below |
| East | Residential Gardens | Tree-lined streets, memories of the old city |
| West | Entertainment District | Neon lights, parties, hidden truths |

## How to Play

### Native Viewer (recommended)
```
VIEW.bat
```

### Vulkan Player
```
ENGINE.bat
```

### Controls
- **WASD** — Move
- **Mouse** — Look around
- **Space** — Interact

## Project Structure

```
four-districts/
├── assets/
│   ├── models/          # 3D assets (.obj + .mtl)
│   ├── scenes/          # Scene graph (world.lscn.json)
│   └── asset_index.json # Asset manifest
├── story/
│   ├── story.md         # Narrative
│   ├── items.json       # Collectibles
│   └── roster.json      # NPCs & enemies
├── world_state.json     # Game state & palette
├── ENGINE.bat           # Vulkan player launcher
├── VIEW.bat             # Orbit viewer launcher
├── NOTES.md             # Design notes
└── ATTRIBUTION.md       # Asset provenance
```

## Generation

Generated using Litt Engine worldgen tools:
```bash
python template/tools/worldgen/gen_archetype.py \
    --archetype open_world_realistic \
    --pattern hub_spoke \
    --theme modern_city_day \
    --seed 1234 \
    --out-dir Project/four-districts
```

## Lore

Before the Fracture, Oakhaven was one city. Then the Great Split divided it into four districts, each with its own council and secrets. You've arrived with a message that could change everything — but first, you must understand what really happened.

## License

Generated for the Litt Engine. All assets procedurally created.
