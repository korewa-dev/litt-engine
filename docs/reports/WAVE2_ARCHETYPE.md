# WAVE 2 — gen_archetype gameplay nodes (audit punch-list item 3)

Task: audit §4 item 3 [M] — "gameplay nodes for EVERY pattern". Before this change only
`hub_spoke` emitted tagged interactive nodes; `corridor_run` baked pickups+goal into
`layout_main` (invisible to the engine), and `arena_ring`/`grid_board` emitted zero
gameplay nodes.

**File touched: `template/tools/worldgen/gen_archetype.py` only** (plus removal of the
one dangling `%TEMP%` registration the regression probe added to `Project/games.json`,
restoring it to its pre-probe state).

## Approach

All three patterns now build an `extras` list using the exact hub_spoke tuple convention
`(emit_name_or_None, ref_model, display_name, mb_or_None, pos, yaw_deg, tags)`; `main()`
already walks that list generically (emits each new mesh once, appends `model:<ref>` tags,
writes nodes). Dispatch in `main()` gained one explicit `spline_track` branch ahead of a
generic `isinstance(payload, tuple)` branch so hub/spline/room_graph payloads are
untouched. Every new prop mesh is built AT ORIGIN and placed purely via node position,
and is guarded at generation time by `worldkit.assert_origin_centered(mb)` (the wave-1
convention guard, audit item 2) — task rule 5 enforced, not just followed.

RNG streams are unchanged: none of the additions consume draws, so obstacle scatter /
tile banding sequences are bit-identical to before.

## Per-pattern changes

### 1. corridor_run (`pattern_corridor`)
- Removed the baked `pickups`/`goal` builders (`co`/`po`) from the `layout_main`
  MeshBuilder; floor/walls/obstacles stay baked.
- New prop mesh `coin.obj`: `octahedron(0,0,0,0.22)` at origin. 10 instance nodes
  `Coin_01`..`Coin_10`, tags `["pickup","model:coin"]`, positions `[5 + i*(length-10)/9,
  1.2, 0]` — the exact unrounded centers the baked run used (visual parity).
- New prop mesh `goal_banner.obj` at origin: original post geometry rebuilt centered
  (`box(0,1.6,0,0.4,3.2,0.4)`) + symmetric crossbar and two hanging cloth prisms.
  Node `Goal_Banner` at `[length-1, 0, 0]`, tags `["goal"]`.
- Probe: 12 nodes total (Corridor + 10 coins + banner), 3 assets.

### 2. arena_ring (`pattern_arena`)
- New prop mesh `aggro_small.obj` at origin: low box body + accent crest pyramid
  (in-pattern build — gen_props kit meshes don't exist during a bare gen_archetype run,
  so referencing them would dangle). 4 nodes `Aggro_Small_01..04` on the mid-ring
  diagonals (r = size/2, clear of pillars r = size-3.5 and center pyramid), yaw facing
  center, tags `["enemy","aggro_small"]`.
- New prop mesh `goal_beacon.obj` at origin: octahedron + hover ring. Exactly ONE node
  `Goal_Beacon` at `[0, 3.6, 0]` hovering over the center pyramid apex, tags `["goal"]`.
- Probe: 7 nodes (Arena_Floor + Ground_Pad + 4 enemies + beacon), 4 assets.

### 3. grid_board (`pattern_grid_board` → actual PATTERNS key `grid_board`, fn
`pattern_board`)
- New prop mesh `pawn.obj` at origin: cyl base + collar cap (silhouette matches the
  gen_tabletop pawn, but origin-centered instead of baked coords). 5 nodes `Pawn_01..05`
  on the near rank `qz = n-1`, `qx ∈ [-2..2]`, y = tile top 0.14, tags
  `["enemy","piece"]`. Tag note: `"piece"` alone is NOT engine-visible (litt_world.c
  entity flags are enemy/hazard/pickup/score/token/dice/objective/scoring/goal/win/
  checkpoint/poi), so `enemy` carries interactivity while `piece` keeps tabletop flavor;
  gen_tabletop's own pawns use `["token","player"]` and are likewise engine-visible only
  through `token`.
- New prop mesh `goal_gate.obj` at origin: two posts + accent lintel. One node
  `Goal_Gate` at `[0, 0.14, -n*1.15]` straddling the far edge tiles, tags `["goal"]`.
- Probe: 7 nodes (Board + 5 pawns + gate), 3 assets.

### Untouched (per task rules)
`spline_track`, `room_graph`, `pattern_hub` bodies; OBJECTIVES dict; identity block and
`state.gameplay` construction in `main()`.

## GATE RESULTS (verbatim)

```
> python -m py_compile template/tools/worldgen/gen_archetype.py
py_compile exit: 0

> python template/tools/worldgen/gen_archetype.py --archetype endless_runner --pattern corridor_run --theme cyberpunk_neon --seed 777 --name probe-corridor_run --out-dir $env:TEMP/litt-ap-corridor_run
[archetype] endless_runner | corridor_run | cyberpunk_neon -> 3 assets, 12 nodes
exit: 0

> python template/tools/worldgen/gen_archetype.py --archetype bullet_hell --pattern arena_ring --theme dark_fantasy --seed 777 --name probe-arena_ring --out-dir $env:TEMP/litt-ap-arena_ring
[archetype] bullet_hell | arena_ring | dark_fantasy -> 4 assets, 7 nodes
exit: 0

> python template/tools/worldgen/gen_archetype.py --archetype grid_tactics --pattern grid_board --theme high_fantasy --seed 777 --name probe-grid_board --out-dir $env:TEMP/litt-ap-grid_board
[archetype] grid_tactics | grid_board | high_fantasy -> 3 assets, 7 nodes
exit: 0

> native/bin/littcli.exe validate $env:TEMP/litt-ap-corridor_run --frames 120
[native] rendered 120 frames | 156 tris | 1 solids | 11 interactives
{"ok":true,"mode":"Orbit3D","frames":120,"solids":1,"interactives":11,"tris":156,"missing":0}
exit: 0

> native/bin/littcli.exe validate $env:TEMP/litt-ap-arena_ring --frames 120
[native] rendered 120 frames | 544 tris | 2 solids | 5 interactives
{"ok":true,"mode":"TopDown","frames":120,"solids":2,"interactives":5,"tris":544,"missing":0}
exit: 0

> native/bin/littcli.exe validate $env:TEMP/litt-ap-grid_board --frames 120
[native] rendered 120 frames | 1956 tris | 1 solids | 6 interactives
{"ok":true,"mode":"Orbit3D","frames":120,"solids":1,"interactives":6,"tris":1956,"missing":0}
exit: 0
```

Goal-tag verification (parsed `assets/scenes/world.lscn.json` per probe):

```
litt-ap-corridor_run | goal nodes: ['Goal_Banner']  | interactives: 11
litt-ap-arena_ring   | goal nodes: ['Goal_Beacon']  | interactives: 5
litt-ap-grid_board   | goal nodes: ['Goal_Gate']    | interactives: 6
```

Determinism (corridor_run seed 777 regenerated into a second dir, SHA256 compare):

```
asset_index identical: True
world.lscn identical: True
```

Regression (full pipeline):

```
> python template/tools/worldgen/make_game.py --about "endless runner neon" --seed 9 --name ap-regress --out-dir $env:TEMP/litt-ap-regress
[make] endless_runner + spline_track would break side-view movement -> corridor_run
[make] building 'ap-regress' | endless_runner/corridor_run/retro_scifi kit=platformer seed=9 -> C:\Users\roika\AppData\Local\Temp\litt-ap-regress
[make] story layer: 14 items -> pickups, 10 roster -> enemies
[native] rendered 120 frames | 156 tris | 1 solids | 43 interactives
{"ok":true,"mode":"Orbit3D","frames":120,"solids":1,"interactives":43,"tris":156,"missing":0}
{"ok": true, "game": "ap-regress", "dir": "C:\\Users\\roika\\AppData\\Local\\Temp\\litt-ap-regress", "objs": 8, "solids_nodes": 1, "play": "ENGINE.bat/.sh", "view": "VIEW.bat"}
exit: 0
```

(43 interactives = 10 coins + goal + story/enrich layers on top of the now-visible
base nodes; pre-fix this pipeline reported 0 interactives from bare generation.)

Cleanup: all five `%TEMP%\litt-ap-*` probe dirs removed (verified 0 remaining); the
`ap-regress` entry make_game appended to `Project/games.json` removed and JSON
re-validated. Acceptance criteria of audit item 3 met for every pattern: interactives > 0
and >= 1 goal-tagged node in every generated scene.
