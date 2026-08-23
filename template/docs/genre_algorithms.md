# Genre Algorithm Encyclopedia

Which math and algorithms make which game genre tick, and how to route them
through Litt tooling. Identity/feel layer: template/tools/worldgen/design_types.json
and design_rules.json. Primitive recipes: procedural_asset_math.md.

**Most of these are now EXECUTABLE CODE** in template/tools/worldgen/algokit.py:
Vec2/Vec3, BSP rooms, cellular caves, A*, BFS flow fields, Bresenham LOS,
Poisson disc, Fisher-Yates, jump-arc solver, Catmull-Rom splines - all
deterministic and unit-checked. Import them; do not re-derive from prose.

## Core algorithms (shared by almost everything)

| Algorithm | Use | Where implemented |
|-----------|-----|-------------------|
| xorshift32 seeded RNG | determinism everywhere | worldkit.Rng |
| value noise + fBm | terrain, biome maps, texture speckle | worldkit.value_noise / fbm |
| dart-throwing scatter with min spacing | trees, rocks, loot spawns | cookbook sec 6 |
| world-space sampled chunk terrain | seamless infinite ground | live_landscape.py / worldkit.emit_chunk |
| Poisson-disc sampling | even non-overlapping placement | cookbook (dart-throw variant) |
| Catmull-Rom spline | roads, race tracks, camera paths | derive points: p(t) = 0.5*((2P1)+( -P0+P2)t+(2P0-5P1+4P2-P3)t^2+(-P0+3P1-3P2+P3)t^3) |

## Per-genre table

| Genre | Signature algorithms | Litt route |
|-------|----------------------|------------|
| Soulslike | stamina economy (costs/regen as data), telegraph windows, corpse-run checkpoint loop, aggro radii | gen_soulslike.py |
| Roguelike dungeon | BSP room partition, cellular automata caves (4-5 rule iterations), A* on tile grid | gen_custom.py: BSP via worldkit.box rooms + corridors |
| Metroidvania | ability lock-key graph BEFORE layout, backfilled shortcuts | gen_platformer25d.py base + gate nodes |
| Platformer 2.5D | jump arc h=v^2/2g, range=v_x*2v_y/g sets gap widths; coyote time; parallax depth layers | gen_platformer25d.py |
| Space salvage/sim | Newtonian thrust v+=a*dt, 6DOF, dart-scattered fields | gen_space.py |
| Tabletop/board | axial hex coords x=s*sqrt3*(q+r/2), z=s*1.5r; movement cost bands; d6 distributions (2d6 bell curve) | gen_tabletop.py |
| Tower defense | BFS flow field from goal, wave budget curves | gen_custom.py: grid + flow tags |
| Card/deckbuilder | Fisher-Yates shuffle, mana curve tables, node-map branching | custom: JSON deck data in state |
| Racing | Catmull-Rom track spline, banking angle from curvature | custom: extrude road boxes along spline samples |
| Open world RPG | POI chains radiating from hubs, level-band rings, landmark sightlines | live_landscape.py base + hub props |
| Survival crafting | biome fBm layers, resource-density inverse to danger, day-night cycle curve, hunger decay | custom: two-fBm overlay in emit_chunk band_fn |
| City builder | L-system road growth, zone demand curves, agent path flows | custom |
| Farming sim | discrete tick growth (logistic curve), season calendar array | custom |
| Bullet hell | pattern parameter timelines (spiral/ring/fan), safe-zone guarantees per phase | custom |
| Horror | light budget per area, inverse-square audio falloff, fog density curves | custom: dark palette + fog_gate style slabs |
| Puzzle (Sokoban-like) | generate reverse from solved state, solvability via reverse BFS | custom |
| Turn-based tactics | cover grid lanes 2-4 m wide, initiative order scheduling | gen_tabletop.py tiles as arena base |
| Extraction shooter | loot value gradient toward center, extraction points at edges, raid timer | custom |

## Identity layer (feel, not structure)

When the human names a FEEL ("like Hades", "cozy farming"), resolve it through:
1. design_rules.json[archetype] -> camera/combat/movement/structure defaults
2. procgen_rules[] there -> concrete steps for this generator
3. tone_and_pacing -> adjust spacing/densities: fast_paced = tighter encounters,
   slow_paced = wider vistas, survival_pacing = resource scarcity

## Determinism contract

Every generator must be reproducible from its recorded seeds alone. No wall-clock
randomness in geometry. Seeds land in LIVE_LOG.md automatically.
