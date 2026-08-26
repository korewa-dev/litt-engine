# NOTES - four-districts

## Game Concept

A city exploration game set in a metropolis divided into four distinct districts:
- **North District** — Financial Quarter (glass & steel)
- **South District** — Industrial Zone (rust & iron)
- **East District** — Residential Gardens (green & living)
- **West District** — Entertainment District (neon & night)

## Generation Info

- Archetype: open_world_realistic
- Pattern: hub_spoke (central plaza + 4 district spokes)
- Theme: modern_city_day
- Seed: 1234
- Built by: gen_archetype.py + custom narrative layer

## Layout

```
          [North District]
               |
               |
[West] -- Central Plaza -- [East]
               |
               |
          [South District]
```

## Play

- ENGINE.bat (Vulkan player) or VIEW.bat (C++ orbit viewer)
- Free roam movement between districts
- WASD to move, mouse to look around

## Story

Explore Oakhaven, uncover the truth behind the Great Fracture,
and find the Charter Hall beneath Central Plaza.

## Files

- `world_state.json` — game state, palette, districts
- `assets/scenes/world.lscn.json` — scene graph with district nodes
- `assets/asset_index.json` — asset manifest
- `story/story.md` — narrative
- `story/items.json` — collectibles
- `story/roster.json` — NPCs and enemies
