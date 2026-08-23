# WorldGen - Genre World Generators for Litt Engine

Pre-built generators so AI agents skip the grunt work, plus a custom scaffold
for everything else. All generators are deterministic (same seed = same bytes),
idempotent, budget-checked, and write index/scene/state/log automatically.

## The layer cake

1. **design_types.json** - the identity vocabulary: cameras, combat, movement,
   structures, narrative styles, archetypes, pacing, signatures.
2. **design_rules.json** - maps each archetype to camera/combat/movement defaults,
   environment types, concrete procgen rules, AI behavior, and WHICH generator to run.
3. **genre_algorithms.md** (in template/docs) - the math behind every genre.
4. **The generators below** - executable implementations.

## Generators

| Command | World | Identity |
|---------|-------|----------|
| python gen_soulslike.py --out-dir Project/live | EMBERFALL HOLLOW - bonfire, hollows, fog gate, boss arena, corpse run | soulslike |
| python gen_space.py --out-dir Project/live | VOID DRIFT - station, asteroids, pods, star canopy | space salvage |
| python gen_tabletop.py --out-dir Project/live | COUNCIL OF SIX - hex board, pawns, dice | tabletop strategy |
| python gen_platformer25d.py --out-dir Project/live | RUSTY CINDER RUN - pits, spikes, coins, goal | 2.5D platformer |
| python live_landscape.py --out-dir Project/live | perpetual grass meadow (original) | ambient/open world |
| python gen_archetype.py --archetype <any> [--pattern P] [--theme T] [--time-of-day D] [--weather W] | ANY of the 76 archetypes via 6 parametric layout patterns; identity from design_rules.json, palette from themes.json, environment block (sky/fog/wind/light) in world_state.json | every archetype in design_rules.json |

**genre_index.csv** - the human-facing index: classic genre > subgenre >
archetype_key (exact design_rules key) > pattern/generator to run > suggested
theme > example games. When a human says "like <game>", look it up here first,
then run the listed command. All keys/themes verified against the datasets.

Common flags: --out-dir (default .), --agent <name>, --prompt "<human request>".
Genre-specific flags exist per generator (--radius, --stars, --asteroids, --seed...).

Run from inside Project/live with no --out-dir to upgrade the live world in place.

## Custom game? Start here

Copy **gen_custom.py** to gen_yourgame.py and edit three marked sections:
PALETTE, build_monument() (geometry), GAMEPLAY (rules as data). The plumbing -
index registration, scene graph, state file, log trail - is automatic.

WorldKit (worldkit.py) provides: xorshift32 Rng, value noise + fBm, MeshBuilder
(box, roof prism, pyramid, cylinder/cone/disc, octahedron, hex tile), MTL writer,
chunk emitter with material banding, index/scene/state/log writers.

## Adding a generator for a new archetype

1. Read design_rules.json - if your archetype exists, follow its procgen_rules.
2. Copy gen_custom.py; rename; set PALETTE and GAMEPLAY first.
3. Build props with cookbook primitives only (see procedural_asset_math.md).
4. Place them via the layout list (name, model, position, yaw, tags).
5. Encode rules as DATA in the gameplay block - viewers and future code read it.
6. Test into an empty temp dir before aiming at a real project folder.

## Hard rules

- Determinism: every random draw through Rng(seed); record seeds in output.
- Budgets: < 500 KB per model (save_prop enforces), palette <= ~12 materials.
- world_state.json is written LAST - viewers poll it and must never see
  references to files that do not exist yet.
- Log every run via append_log with agent name and human prompt.
