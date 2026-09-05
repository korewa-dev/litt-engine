# WAVE_PLATFORMER — gen_platformer25d.py fixes (WORLDGEN_AUDIT items 19–21)

Date: 2026 session wave · File touched: `template/tools/worldgen/gen_platformer25d.py` only
(make_game.py untouched; its place_on collision work is a later wave).

## Item 19 — Honor `--seed`
- All layout numbers now derive from `worldkit.Rng(seed)` in one fixed draw
  order: gap count (4–5) → per-gap run (11–16 m) + width (3.0–4.6 m) →
  platform count (5–7) + x/y scatter → per-platform deck variant → per-coin
  arc offsets/heights.
- Gap cap is physics-derived: `algokit.solve_jump_arc(12, 30, 8)` gives max
  range 6.4 m; every width re-checked with `algokit.can_clear_gap` (assert).
- Same seed ⇒ byte-identical outputs (models, MTL, index, scene, state;
  `world_state.json.updated` is now a seed-derived deterministic timestamp).
  Different seeds ⇒ different layouts. Verified with seeds 111 vs 222.

## Item 20 — De-double-transform + instancing
- Single convention: every mesh built AT ORIGIN (`save_prop(enforce_origin=
  True)`), placement carried solely by scene `node.position`.
- Coins: ONE `coin.obj`, 15 instance nodes. Platforms: three reusable decks
  (`platform_short/mid/long`, 2.0/2.6/3.2 m) instanced at seed-driven spots.
- Level track remains a single level-node mesh; its vertices are relative to
  the node pivot (was: absolute coords + offset position = ~2x displacement).
- OBJ count dropped from ~20+ per-generate to exactly 10.

## Item 21 — Hazard node emission
- Pit slabs and spike cones removed from the baked track mesh; each gap now
  emits `Hazard_Pit_NN` + `Hazard_Spike_NN` nodes (origin-centered meshes,
  node at mid-gap y=-1.5 so the engine's interact-radius kill sphere triggers
  inside the pit but never clips legitimate jumps).
- Tags: `["hazard","pit"]` / `["hazard","spikes"]` — matches
  `native/littcore/litt_world.c` LV_F_HAZARD handling (kill/respawn). No fake
  pickup/enemy tags emitted; coins keep `["pickup","score"]`, flag keeps
  `["goal","win"]` (exactly one goal node).
- `state.gameplay.hazards` prose rewritten to document the hazard nodes.

## Gate results (all pass)
- `python -m py_compile template/tools/worldgen/gen_platformer25d.py` → exit 0
- seeds 111/222 into %TEMP%: exit 0 both; `world.lscn.json` diff = 106 lines;
  same-seed rerun byte-identical over all 14 non-log files.
- lint_game: `objs: 10 | problems: [] | dangling: []`
- `native/bin/littcli.exe validate <dir> --frames 120`:
  `{"ok":true,...,"solids":1,"interactives":26,"tris":72,"missing":0}` (exit 0)
- Tags: 10 hazard / 15 pickup / 1 goal-tagged node.
- Platform centroid spot-check: 3/3 at |centroid| = 0.0000 (< 1.5), placement
  in `node.position` (e.g. Platform_01 pos [96.08, 1.8, 0.0]).
- Probe dirs cleaned from %TEMP%.
