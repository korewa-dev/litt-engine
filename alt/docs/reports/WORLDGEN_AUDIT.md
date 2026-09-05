# WORLDGEN AUDIT — template/tools/worldgen/

Scout audit for the worldgen rebuild effort (read-only + cheap probe runs).
Scope: algokit.py, worldkit.py, design_rules.json, design_types.json,
design_extra.json, themes.json, genre_index.csv, gen_archetype.py,
gen_custom.py, gen_soulslike.py, gen_space.py, gen_tabletop.py,
gen_platformer25d.py, gen_props.py, gen_story.py, gen_launchers.py,
enrich_game.py, merge_rules.py, make_game.py, README.md.

Probe evidence (all reproducible):
- `py_compile` on all 14 .py files: everything passes EXCEPT gen_soulslike.py
  → `SyntaxError: invalid syntax` at line 132 (`for k in` truncated mid-loop).
- Full pipeline: `python template/tools/worldgen/make_game.py --about "souls knight dungeon" --seed 42 --name scout-probe --out-dir %TEMP%\litt-scout-probe`
  → exit 0; last line `{"ok": true, "game": "scout-probe", ...}`.
  littcli: `{"ok":true,"mode":"TopDown","frames":120,"solids":1,"interactives":31,"tris":628,"missing":0}`.
- Determinism: second identical run → scene JSON and layout_main.obj byte-identical.
- Render: `native/bin/littview.exe render <dir> --out probe.bmp` → ok, 2978 tris, 960x540 BMP, missing=0.
- Repo left clean: Project/games.json snapshotted before probes and restored after; probe outputs live only in %TEMP%.

---

## 1. Pipeline contract (README.md + make_game.py)

Entry point:

```
python template/tools/worldgen/make_game.py --random
python template/tools/worldgen/make_game.py --about "a game about zombie mall survival"
```

Overrides: `--name --seed --archetype --pattern --theme --kit {survivor|platformer|souls}
--time-of-day {dawn|noon|dusk|night} --weather {clear|rain|snow} --scale {small|medium|full}
--out-dir --skip-validate`. `--help` works.

Per-game chain (each step a shipped tool):

1. Intent mapping: regex table over `--about` text → (archetype, pattern, theme, kit);
   `pick_random` for `--random`; scale inferred from wording, default "medium".
2. Geometry: `gen_platformer25d.py` ONLY if archetype == "platformer25d";
   otherwise `gen_archetype.py --archetype A --pattern P --theme T --seed S`.
   Side-view kits force pattern corridor_run if spline_track/room_graph were chosen.
3. Props: `gen_props.py --game-dir <out> --kit <kit>` (shared prop library).
4. Layout derivation: make_game's own `scene_layout()` parses
   assets/scenes/world.lscn.json + OBJ vertex bounds of nodes tagged
   floor|level|track|board|hub|terrain → union bbox + long axis; `place_on()`
   puts brief anchors at fractions along that axis at bbox top + lift.
5. Brief auto-authoring: kit templates (platformer / souls / survivor) produce
   brief.json with objective, side_objectives, physics dict, lives, score_goal,
   waves, roster, spawn, checkpoints, zones, and gameplay nodes tagged
   pickup|score|enemy|hazard|goal|checkpoint|poi plus `model:<ref>` refs —
   refs are filtered against models actually shipped by the prop kit (`have_any`).
6. Story layer: `gen_story.py --about --game-dir --archetype --scale --seed`
   writes story/items.json + roster.json; items → pickup nodes, roster →
   Mook_/Elite_/Boss_ enemy nodes (model refs filtered against shipped meshes).
7. Enrich: `enrich_game.py --game-dir --brief brief.json --seed S`
   applies brief → scene nodes (idempotent by name) + state.gameplay block;
   writes scene FIRST, world_state.json LAST (viewers poll state).
8. Validate: `lint_game(out)` + `solid_count(scene)` from template/tools/assets/lint.py;
   problems or dangling refs → JSON failure line + exit 1.
9. Native validation: `native/bin/littcli(.exe) validate <dir> --frames 120`
   (60 s timeout); if binary missing, fallback `<game>/play_native.py --project <dir> --frames 30 --dummy`.
10. Deploy: play_native.py copied in (from Project/example-village), VIEW.bat
    (hardcoded ..\..\native\bin\littview.exe), VALIDATE.bat, ENGINE.bat/.sh via
    gen_launchers, NOTES.md, ATTRIBUTION.md, registration in Project/games.json.
    Last stdout line is machine-readable JSON {"ok":true,...}.

IMPORTANT ORPHANING FACT: README documents gen_soulslike / gen_space /
gen_tabletop as flagship generators, but make_game NEVER invokes them — only
gen_archetype + gen_platformer25d are wired. design_rules.json carries a
`generator` field per archetype, but it is unparseable prose (66x "custom",
others like "gen_soulslike.py --radius 1", "custom - BSP rooms via
worldkit.box") and no code reads it.

---

## 2. Per-file classification

| File | Verdict | Specific defects |
|---|---|---|
| worldkit.py | OK | Solid core: xorshift32 Rng, value noise/fBm, MeshBuilder (box/prism/pyramid/cyl/cone/octahedron/hex_tile/sphere), budget-checked save_prop (<500 KB), index/scene/state/log writers, auto `model:` tag injection, theme loader. Gaps vs objective: NO AABB/placement registry or any collision query; write_state-last is convention, not enforced; nothing guards the baked-vs-node transform convention. |
| algokit.py | OK as library / DEAD in practice | Correct implementations (Vec2/Vec3, BSP, cellular caves, A*, BFS flow fields, Bresenham LOS, Bridson Poisson-disc, Fisher-Yates, jump-arc solver, Catmull-Rom). ZERO importers anywhere in the repo — generators re-hardcode its math instead (platformer jump constants, raw uniform scatters). Minor: mixed rng protocol — needs worldkit.Rng (next_u32) not random.Random. |
| gen_archetype.py | PARTIAL | The workhorse (76 archetypes x 6 patterns x 26 themes). Good: identity block from design_rules into state.identity, structured environment block (sky/sun/fog/weather/time-of-day), deterministic Rng(seed), hub_spoke emits correctly-instanced gameplay nodes (coins/stalkers/banner share one mesh). Defects: (a) gameplay wiring exists ONLY in hub_spoke — corridor_run bakes pickups+goal INTO layout_main mesh with NO scene nodes/tags (engine cannot see them); arena_ring and grid_board emit ZERO gameplay nodes; (b) room_graph picks rooms with rng.uniform()>0.25 — connectivity NOT guaranteed, loot can be isolated; (c) spline_track closed Catmull-Rom loop through random control points — self-intersection unchecked; (d) DOUBLE-TRANSFORM bug class: hub POI stones and start gate bake world coords into vertices AND set node.position to the same coords; (e) scatter avoids path corridors but has no overlap checks. |
| gen_platformer25d.py | PARTIAL→BROKEN | (a) `--seed` accepted but NEVER USED — GAPS/platforms/coins fully hardcoded constants (deterministic but seed-invariant); (b) SYSTEMIC double-transform: every prop (level track, platforms, coins, backdrops, flag) bakes absolute coords AND sets node.position to the same coords → renders displaced ~2x under transform-applying consumers; (c) spikes/pits baked into the track mesh with NO hazard nodes/tags — hazards exist only as prose in state.gameplay.hazards; (d) one OBJ per coin/platform/backdrop (12+ tiny files) instead of instancing; (e) jump math hardcoded in comments/constants instead of algokit.solve_jump_arc/can_clear_gap; (f) no direct enrich/brief integration of its own (relies on make_game). |
| gen_soulslike.py | BROKEN | Fatal SyntaxError line 132: `for k in` — file truncated mid-loop, cannot even byte-compile, so the soulslike flagship is DEAD today. Content up to the cut was sound: bonfire/checkpoint, corpse-run bloodstain, hollows (enemy+aggro tags), fog gate, boss knight, soul-ember pickups; fBm terrain planned via emit_chunk. Also uses module-const seeds (SEED_T/SEED_S) — `--seed` would need plumbing. |
| gen_space.py | PARTIAL | Deterministic (Rng(seed)) and decent composition for station/pods. Defects: (a) asset explosion — ONE OBJ PER STAR (default 280!) plus one per asteroid/pod; (b) placement is pure uniform random ±40 with NO collision checks — asteroids can overlap the station ring (r≈3.4) or each other, pods can spawn inside rocks; (c) no physics dict (movement is a prose string); hazards are a prose list, not node tags; pods tagged objective/salvage, never goal; (d) void plane node doubles as backdrop+terrain+floor (giant disc = one solid). |
| gen_tabletop.py | PARTIAL | Board itself fine: seeded fBm terrain bands over 37 hexes in one mesh, wooden frame. Defects: (a) pawn meshes bake x,z AND node pos x,z → double-transform; (b) win condition/tile move costs are prose strings in state.gameplay — no goal/pickup/hazard NODE TAGS anywhere; (c) one OBJ per pawn/die (8 files). |
| gen_props.py | OK | Multi-part props with real composition (drone: hull+canopy+4 rotors; brute walker; knight; bonfire…), palette MERGE with existing materials.mtl (prop_* prefix, never recolors built meshes), idempotent skip of existing objs, index registration, budget check. Nit: dead `part()` helper, unused `pal` arg (prefix hardcoded). |
| enrich_game.py | OK | Idempotent node adds (name-checked), correct scene-then-state write order, zones via scale.x*10 convention, story poi flavor passthrough. Trusts brief blindly, but dangling model: refs are caught downstream by lint. |
| make_game.py | PARTIAL (works E2E) | Probe passed end-to-end. Defects vs objective: (a) no native_proof pixel-assertion gate — only littcli validate; (b) `place_on()` fractional-spine placement has zero collision/ground checks — anchors can land beside narrow decks or inside walls; (c) deploy_runtime dead code (unused `rt`, unused port_seed param, viewer/ dir created but never filled) and fragile paths: VIEW.bat hardcodes ..\..\native\bin\littview.exe (only valid for REPO/Project/<name> two-deep layout), play_native.py copied from Project/example-village (hard dependency on example project); (d) dead `{"rooms_pattern":"room_graph"}` mapping; (e) INTENT keyword table small relative to 76 archetypes. |
| gen_custom.py | OK | Clean minimal scaffold (chunked fBm terrain + monument). Nit: chunk files reused if they already exist regardless of seed → stale chunks can survive a re-gen into same dir. |
| gen_story.py | OK (supporting) | Emits story/story.md + items.json + roster.json consumed by make_game; failures treated as non-fatal upstream. |
| gen_launchers.py | OK | ENGINE.bat/.sh generation, LITT_ENGINE env → release → debug resolution. |
| merge_rules.py | OK | Idempotent merge of design_extra.json into design_rules.json (+N new, skips present). |
| design_rules.json / design_types.json / design_extra.json / themes.json / genre_index.csv | OK (data) | 76 archetypes verified, identity vocabulary consistent, genre_index.csv keys match. One defect: `generator` field values are ad-hoc strings (see §1 orphaning fact). |

---

## 3. Validation commands found

Python lint (always run inside make_game, imported from template/tools/assets/lint.py):

```python
from lint import lint_game, solid_count   # sys.path includes template/tools/assets
lint_game(dir)     # bare-usemtl, face-index>vcount, duplicate node ids, next_id, dangling model:<ref>
solid_count(scene) # counts nodes tagged floor|level|board|track|hub|terrain|platform
```

Native validator (make_game step 9; binary exists at native/bin/littcli.exe):

```
littcli validate <game-dir> --frames 120        # make_game, timeout 60 s
littcli validate <game-dir> --frames N          # general contract (native/littcli.c)
# fallback when binary missing:
python <game>/play_native.py --project <game> --frames 30 --dummy
# must print interactives > 0 and solids > 0 (platformers)
```

Pixel-proof gate (NOT currently called by make_game or any worldgen tool;
called only by tools/litt.py and studio/cs/LittStudio.cs):

```
python template/tools/assets/native_proof.py [--min-fill F]
# per Project/* game having world_state.json AND story/:
#   1. littcli validate <dir> --frames 60
#   2. littview render <dir> --out tmp.bmp  -> asserts frame fill %, >=8 color
#      families (/24 buckets), vertical span; exit 0 only if ALL games pass
```

Docs references: native/build.bat (`littcli.exe validate ..\..\Project\live --frames 30`),
template/docs/game_type_generation.md (`native/bin/littcli validate <project> --frames 60`),
editor/README.md (`native\bin\windows\littcli.exe validate Project\live --frames 30`).

---

## 4. PUNCH LIST (ordered)

Sizes: S = one small focused change (~<=1 h agent task), M = half-day coherent
task, L = multi-part day+. Ordering: [SHARED] first because later items depend
on the registry/transform helpers; within groups, fix-broken before improve.

### [SHARED] — worldkit / algokit / make_game

1. [S] worldkit: add AABB placement registry — `Placement.insert(name, bounds)` that rejects/relocates overlaps and a ground-snap helper returning bbox-top Y for (x,z). Thread through save_prop/write_scene callers so every generator gets collision-safe placement for free.
2. [S] worldkit: enforce the transform convention — helper that builds props AT ORIGIN and places them purely via node.position; add an assertion (mesh centroid ~ origin) so the double-transform bug class cannot recur. This unblocks items 6, 14b, 17, 20.
3. [M] gen_archetype: gameplay nodes for EVERY pattern — split corridor_run pickups/goal out of layout_main into instanced tagged nodes (reuse the hub extras mechanism); add enemies + goal nodes to arena_ring; add pawns/goal nodes to grid_board. Acceptance: every pattern yields interactives>0 and >=1 goal-tagged node in littcli output.
4. [M] gen_archetype room_graph connectivity — replace rng cell-picking with BSP (algokit.bsp_partition) or MST over chosen cells; verify reachability of every room + loot with algokit.bfs_flow_field/astar; regen or bridge isolated rooms (seeded).
5. [S] gen_archetype spline_track robustness — closed-loop self-intersection test on sampled polyline + minimum clearance between road boxes; regenerate control points on failure (seeded retry).
6. [S] gen_archetype double-transform fixes — build hub POI stones and start gate at origin, place via node.position only.
7. [M] make_game collision-aware brief placement — replace place_on spine-fraction math with registry queries: snap spawn/checkpoints/items/enemies to floor top, enforce min spacing, reject wall overlaps; fall back to safe defaults when layout derivation fails.
8. [S] make_game native-proof gate — after littcli validation, run native_proof assertions for the single new game (or factor its bmp_stats into a reusable function) and include fill%/colors in the final machine JSON.
9. [S] make_game deploy_runtime cleanup — delete dead `rt`/port_seed/viewer-mkdir code; generate VIEW.bat through gen_launchers-style templating with robust relative path (or absolute-from-repo detection); ship play_native.py from template/tools/runtime instead of copying out of Project/example-village.
10. [S] design_rules.json generator field — normalize to parseable enum+args ({archetype, platformer25d, soulslike, space, tabletop, custom} + optional args); update merge_rules.py; make gen_archetype/make_game honor it (this also decides item 12's dispatch question).
11. [S] algokit adoption + rng shim — give worldkit.Rng a random.Random-compatible facade (or vice versa); switch scatters to poisson_disc_points and platformer gap math to solve_jump_arc/can_clear_gap wherever generators place things randomly.

### [GEN_SOULSLIKE]

12. [S] Repair the fatal SyntaxError — finish the truncated ember-scatter loop at line 132; py_compile must pass; smoke-run `gen_soulslike.py --out-dir %TEMP%\...` end-to-end.
13. [M] Seed plumbing — route --seed into layout()/prop variation (currently module consts SEED_T/SEED_S); two different seeds must produce different worlds, same seed byte-identical.
14. [L] Composition + wiring rebuild — rebuild props on the gen_props souls kit (bonfire/knight/stalker/estus/banner) with registry placement instead of bespoke one-off meshes; verify full tag contract: checkpoint (bonfire), enemy+aggro (hollows), boss (knight), pickup (embers/estus), goal/fog-gate semantics, corpse_run data in state.gameplay; prove via littcli validate + native_proof assertions.

### [GEN_SPACE]

15. [S] Kill the star/asteroid OBJ explosion — one star mesh + N instance nodes (the coin-instancing pattern from hub_spoke), 3–5 asteroid variant meshes + instances. 285+ OBJs should collapse to ~8.
16. [M] Placement + gameplay semantics — poisson-disc scatter with station exclusion radius (ring r≈3.4 + margin) and asteroid/pod min distances via the shared registry; retag pods with goal/objective semantics; replace prose movement/hazards with a structured physics dict + hazard node tags.

### [GEN_TABLETOP]

17. [S] Fix pawn double-transform — build pawn/die meshes at origin, position via node only.
18. [M] Gameplay wiring — opposite board edges get tagged goal nodes (win condition becomes machine-readable); tile kind + move-cost data as structured state.gameplay.tiles (not prose); optional pickups on plains tiles; keep dice/token tags consistent.

### [GEN_PLATFORMER25D]

19. [S] Honor --seed — derive gaps/platform heights/coin arcs from Rng(seed) within can_clear_gap caps (max range 6.4 m at current physics); same seed reproduces bytes exactly.
20. [M] De-double-transform + instancing — single convention across level/platforms/coins/backdrops/flag; coins share one mesh via instance nodes; platform variants reduced to a few reusable meshes.
21. [S] Hazard node emission — pit spans and spike clusters become nodes tagged hazard (+kill_volume style semantics) so the engine registers them; keep state.gameplay.hazards prose in sync as documentation.
22. [S] End-to-end verification — run the full make_game platformer path: play_native prints solids>0 and interactives>0; littcli validate clean; native_proof pixel assertions pass; record seed + commands in NOTES.md.

Sequencing note: items 1–2 are prerequisites for 6, 7, 14, 16, 17, 20. Item 3
is the highest-value wiring fix — corridor_run is the pattern make_game forces
for side-view kits, and today its pickups/goals are invisible to the engine.
