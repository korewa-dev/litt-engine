# WAVE_ARCHETYPE_456 — gen_archetype items 4+5+6

Scope: `template/tools/worldgen/gen_archetype.py` ONLY. Items 1-3 worldkit/
corridor/arena/board behavior untouched (regression smokes below). CLI
unchanged; same seed -> byte-identical outputs proven per pattern.

## ITEM 4 [M] room_graph connectivity
- `choose_rooms(rng)` - original seeded picker extracted (same rng draws).
- `plan_room_connectivity(chosen)` - BFS components over chosen cells;
  greedy closest-pair component merge (= MST over room centers,
  deterministic index tie-breaks) yields minimal L-corridor links; portals =
  adjacency doorways + every perimeter a corridor threads.
- `corridor_cells(a,b)` - cell-wise L-path (x leg then z); legs run along
  room-center rows/columns so every wall crossing lands inside the existing
  central door gap (no new wall geometry language needed).
- Corridors emit as 1.8 m walkway tiles flush with room floors (y top 0.15);
  walls now draw their two-half gap whenever `_edge_key(cell, nb)` is in
  portals (superset of the old `linked` behavior).
- `room_reachability(chosen, portals, links)` - BFS over rooms + one node
  per corridor (endpoint rooms + threaded rooms); pattern asserts full
  coverage BEFORE emitting (`RuntimeError` seatbelt; planning is
  constructive so it never fires).
- Gameplay wiring via item-3 extras convention (baseline probe scored
  interactives:0 / ok:false without this): baked loot octahedra ->
  instanced `Loot_Gem_NN` ["pickup"] nodes (origin-built `loot_gem.obj`),
  plus goal-tagged `Exit_Gate` ("exit_gate.obj") in the last chosen room.
  rng stream parity with the old loot loop preserved.

## ITEM 5 [S] spline_track robustness
- `condition_points(points, min_sep=0.5)` - drops non-finite points and any
  point < min_sep from the last accepted (first of each near-dup run wins).
- `unit_tangent(p,q)` - guarded direction, None on zero-length/non-finite
  segment (no division by ~0 anywhere in the pipeline).
- `thin_polyline(samples, min_seg=0.05, cap=240)` - skips degenerate
  segments via unit_tangent, clamps sample count (TRACK_MAX_SAMPLES).
- `track_control_points(rng)` - conditioned draws, >=8 seeded retries, then
  canonical ring fallback (always >=4 well-separated points).
- `sample_closed_spline(ctrl)` - closed Catmull-Rom over n=len(ctrl)
  (was hardcoded %6); checkpoint stride guarded max(1, len//4).

## ITEM 6 [M] double-transform fixes (hub + start gate)
- POI stones: cyl/octahedron rebuilt at origin, `assert_origin_centered`;
  node.position carries spoke-end placement.
- Festival banner: pole/cloth rebuilt at origin (cloth re-centered z 0.75->0
  to hug origin), node moved [0,0,0] -> [reach+4, 0, 0].
- spline start gate (audit defect d names it alongside hub stones): posts/bar
  rebuilt at origin, Start_Line node keeps [sx, 0, sz].
- coin/stalker meshes unchanged but now assert_origin_centered-guarded.
- Plaza disc stays baked as the level mesh (sanctioned).
- Known remaining (out of scope, flagged for follow-up): corridor_run's
  "Corridor" and room_graph's "Dungeon" LEVEL nodes carry non-zero positions
  over world-baked layout_main verts - audit item 6 does not list them and
  make_game scene_layout() may depend on them; not touched.

## GATES (verbatim)
```
PY_COMPILE_OK
lint[room]: CLEAN    validate[room]: {"ok":true,"mode":"TopDown","frames":120,"solids":1,"interactives":5,"tris":396,"missing":0}
lint[track]: CLEAN   validate[track]: {"ok":true,"mode":"Orbit3D","frames":120,"solids":2,"interactives":1,"tris":780,"missing":0}
lint[hub]: CLEAN     validate[hub]: {"ok":true,"mode":"Side2D5","frames":120,"solids":2,"interactives":24,"tris":740,"missing":0}
seed=1 rooms=3 links=2 portals=0 reached=3/3 ok=True
seed=2 rooms=5 links=0 portals=5 reached=5/5 ok=True
seed=3 rooms=5 links=1 portals=4 reached=5/5 ok=True
seed=4 rooms=7 links=0 portals=8 reached=7/7 ok=True
seed=5 rooms=6 links=0 portals=6 reached=6/6 ok=True
CONNECTIVITY_ALL_OK
conditioned control points (6): [(0.0,0.0),(5.0,0.0),(10.0,0.0),(15.0,0.0),(20.0,0.0),(25.0,25.0)]
sampled polyline: 60 pts ... all outputs finite: True / SPLINE_DEGENERATE_OK
poi_01: |c_xz|=0.0000 |c_full|=1.900 node.position=[26.0, 0.0, 0.0] ok=True
festival_banner: |c_xz|=0.0000 |c_full|=2.551 node.position=[30.0, 0.0, 0.0] ok=True
start_line: |c_xz|=0.0000 |c_full|=2.017 node.position=[-29.97, 0.0, -23.43] ok=True
HUB_CONVENTION_OK   (strict full-vector reading also <1.5: coin .550, exit_gate 1.195, loot_gem .800)
determinism[room]: BYTE-IDENTICAL   determinism[track]: BYTE-IDENTICAL   determinism[hub]: BYTE-IDENTICAL
smoke[arena]: lint=CLEAN {"ok":true,...,"interactives":5}   smoke[board]: lint=CLEAN {"ok":true,...,"interactives":6}
```
Probe conventions: roguelike/high_fantasy/room_graph, kart_racer/
cyberpunk_neon/spline_track, collectathon_3d/minimalist_abstract/hub_spoke,
seed 7, all under %TEMP%, ALL dirs + throwaway scripts removed after gates.
