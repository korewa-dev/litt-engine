# WAVE: GEN_TABLETOP — audit items 17 + 18

Scope: only `template/tools/worldgen/gen_tabletop.py`. Seed protocol
(`worldkit.Rng`) untouched; same seed => byte-identical outputs (proven below).

## Item 17 — pawn/die double-transform (DONE)

- Pawn body/head cylinders and die cubes are now modeled **AT ORIGIN**
  (`x=z=0`); placement carried purely by `node.position`.
- `save_prop(..., enforce_origin=True)` added on every prop write so the
  worldkit transform-convention guard hard-fails any regression.
- Board hexes/frame untouched (baked coords + identity node = sanctioned
  terrain-style exception per audit).
- Verified: Pawn_01/06 + Die_01 OBJ vertex x/z centroid = 0.0000 while scene
  node positions carry e.g. `[-2.468, 0.0, 4.275]`.

## Item 18 — gameplay wiring (DONE)

- **Goal nodes:** `Goal_North` / `Goal_South` placed on deterministic
  opposite-edge tiles (min-z / max-z rows, tie-break |x| then x), tagged
  `["goal","poi","edge"]`, sharing ONE origin-centered gold banner mesh
  (`goal_banner.obj`) via the extras/instancing pattern (explicit
  `model:goal_banner` refs). Win condition now machine-readable; prose
  fields (`turn_structure`, `win_condition`, tiles legend) kept intact.
- **Structured tiles:** `state.gameplay.tiles` = list of 37 records
  `{q, r, pos:[x,z], kind, cost}` derived from the existing `board_tiles`
  collection (no parallel data structure); cost map `MOVE_COSTS =
  {water:0(blocked), plains:1, forest:2, mountain:3}` matches the retained
  `tiles_legend` prose.
- **Pickups:** up to 4 `Token_Gem_*` octahedron instances tagged
  `["pickup"]` on free plains tiles (pawn-corner + goal tiles reserved),
  one shared origin-centered `token_gem.obj`.

## Enabling fix discovered en route (documented, minimal)

`tile_kind()` produced **tile_water for all 37 hexes at every seed**: noise
inputs (`q*0.5+9`) hashed into one low lattice bucket, so boards were
monochrome and every tile "blocked", making structured costs/goal semantics
vacuous. Reparameterized to `fbm(q*3.1+40, r*3.1+55, seed)` — still a pure
deterministic function of (q,r,seed); seed 777 census now
`{water:4, plains:8, forest:8, mountain:17}`, all four kinds at seeds
42/1337/2024/7 as well. Thresholds unchanged.

## Gates (all passing)

```
py_compile exit=0
probe exit=0                       (%TEMP%\litt-tt-smoke, "[tabletop] ready: 11 assets, 15 scene nodes")
lint exit=0                        {"objs": 11, "problems": [], "dangling_refs": []}
{"ok":true,"mode":"Orbit3D","frames":120,"solids":1,"interactives":13,"tris":900,"missing":0}   validate exit=0
scene asserts exit=0               (2 goal nodes z=-4.275/+4.275; 37 tile records w/ kind+cost; centroids ~0)
assets\asset_index.json: BYTE-IDENTICAL
assets\scenes\world.lscn.json: BYTE-IDENTICAL        (same-seed rerun, SHA256)
```

Probe dirs `%TEMP%\litt-tt-smoke*` deleted afterward.
