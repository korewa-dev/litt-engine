# ASSET AUDIT — the asset creation stack vs CDR-009 doctrine

Read-only scout audit (CDR-009 dispatch). Scope: gen_props.py, worldkit.py,
kit consumers (gen_soulslike / gen_space / gen_tabletop / gen_platformer25d /
gen_archetype), lint.py + native_proof.py, and the material path end-to-end
down into the native renderers. All paths relative to repo root; line numbers
verified on disk today. Several generators are mid-flight (items 3–6, 13/14,
16–21 recently landed); in-flight inconsistencies are marked as such.

**Doctrine under audit** (COUNCIL_DECISIONS.md:149–167):

1. Reusable kit pieces (versionable kits, never bespoke meshes inside a generator)
2. Instance-level editability (origin-centered meshes + node transforms; no baked compositions)
3. Terrain-conditioned placement (query_height + Placement registry; nothing floating/clipping/overlapping)
4. Procedural materials (mat_at convention, deterministic seeded variation, no binary texture deps)
5. Render-based refinement (validate + render + bmp_stats critic; failing assets regenerated/fixed)

**Headline:** the *conventions layer* (worldkit) is now excellent and the
recently-rebuilt generators mostly honor it — but the stack has two live
double-transform bugs the conventions were built to kill, three of five
generators have never touched the kit library, **not one byte of authored
material data reaches a pixel**, and the render critic can fail a project but
nothing regenerates.

---

## 1. Principle 1 — Reusable kit pieces — RATING 2/5

### Current state

* gen_props.py ships 3 kits × 6–7 props with real multi-part composition
  (`KITS` :187–191; drone hull+canopy+eye+arms+4 rotors :75–91; knight 8 parts
  :153–169; bonfire 5 parts :130–141). Budget-checked, index-registering,
  idempotent (:240–251). This half is genuinely good.
* Exactly ONE consumer: gen_soulslike.py:47 imports `PALETTES, build_prop,
  parse_mtl` and emits 5 souls-kit props (:281–284).
* gen_archetype, gen_space, gen_tabletop, gen_platformer25d: **zero
  gen_props imports** (repo grep). Every mesh is a bespoke builder inside the
  generator.
* Vocabulary split is real, not cosmetic: archetype's "stalker"
  (gen_archetype.py:201–205, box+pyramid hood) is a different creature from
  gen_props' stalker (:142–152, cylinder+hood+blades); same for coin, banner,
  pawn. Two engines, two truths for the same noun.

### Gaps

* Kit coverage is far below what generators actually need: no goal gate,
  fog gate, hazards (pit/spike), platform decks, hex pawn, star, asteroid,
  station — each generator rebuilds its own.
* No versioning story: `--force` overwrite is the only mutation path; kits
  are not referenced by version anywhere.
* gen_props takes no `--seed` (argparse :214–221) although its own docstring
  advertises `[--seed N]` (:13) — stale contract, zero per-instance variation.
* Dead code: `part()` helper (:51–53) returns its arg; `pal` param of
  `build_prop` (:45) is unused (prefix hardcoded via `pal_get` :47–48) — the
  signature lies about theming.

### Fix items

| # | Item | Effort |
|---|---|---|
| 1.1 | gen_props v2: absorb the common bespoke meshes (goal_gate, fog_veil, hazard_pit, hazard_spikes, hex_pawn, token_gem, star_glint, asteroid variants, platform decks, ruin arch/pillar) as named kit pieces | [M] |
| 1.2 | Add `--seed` to gen_props; thread one Rng into build_prop for deterministic per-piece variation (sizes/jitter within silhouette bounds) | [S] |
| 1.3 | Replace PALETTES with themes.json-sourced palettes (one theme vocabulary for gens + kits); delete dead `part()` and the fake `pal` param | [S] |
| 1.4 | Migrate gen_archetype extras + gen_tabletop pawns/dice/banners onto kit refs; keep pattern-specific meshes by promoting them INTO the kit | [M] |

---

## 2. Principle 2 — Instance-level editability — RATING 3/5

### Current state

* The enforcement toolbox is complete and well-documented:
  `assert_origin_centered`/`mesh_centroid`/`recenter_mesh`/`TransformError`
  (worldkit.py:388–444), `save_prop(enforce_origin=True | auto_recenter=True)`
  (:605–639), sanctioned terrain-chunk exception spelled out twice
  (:20–22, :461–464). write_scene validates footprints batch-wise (:551–585).
* gen_platformer25d is the model citizen: every prop enforced at origin
  (:112–117), track built pivot-relative so node carries placement
  (:125–128), coins/decks instanced (:131–139, :141–154), hazards are nodes
  (:156–172). Items 19–21 landed clean.
* gen_tabletop items 17/18 landed: pawns/dice/goal_banner/token_gem all
  `enforce_origin=True` (:101–113, :125–126, :143–144).
* gen_space: instancing contract held (1 star mesh + 280 nodes :136–142,
  4 asteroid variants, 1 pod mesh), star+gate `enforce_origin=True`.
* gen_archetype asserts origin on ALL extras meshes (e.g. :90, :101, :126,
  :136, :164, :185, :204, :217, :242, :252, :354, :570, :584) — item 6 class
  fixed for POI stones/start gate.
* gen_soulslike: kit props take `auto_recenter=True` because kit meshes were
  authored slightly off (comment :277–280, bonfire z=-0.06); acceptable, but
  it means the kit itself is not convention-clean.

### VIOLATIONS STILL LIVE (renderer-proven)

littview adds node.position/yaw to every vertex (littview.cpp:251–263), and
litt_world.cpp does the same for the sim (:206–211). Therefore any
baked-coords mesh under a non-identity node is displaced ~2× in BOTH the
render and the gameplay sim TODAY:

* **Terrain chunks**: gen_soulslike.py:270 places chunk nodes at
  `[x*CHUNK, 0, z*CHUNK]` over world-space `emit_chunk` vertices — directly
  against worldkit's documented rule (:461–464 "chunk node MUST sit at
  identity"). Same bug in gen_custom.py:80. Emberfall Hollow terrain renders
  and sims double-offset right now. (lint/littcli cannot see this; it passed
  item-12 verification because those gates count, they don't look.)
* **gen_archetype corridor_run**: floor/walls baked at x=length/2 (:109–112)
  while node sits at `[length/2, 0, 0]` (:139). Double.
* **gen_archetype room_graph**: floors/walls/corridors baked at cell offsets
  (:506, :518–520, :530–554) while Dungeon node sits at
  `[cols*CW/2, 0, rows*RW2/2]` (:588). Double.
* arena_ring / hub_spoke / grid_board / spline_track level meshes are fine
  (identity nodes) — the exception pattern, correctly used.

In-flight note: items 4–6 landed the connectivity/spline/double-transform
work but were scoped to POI stones + start gate; corridor/room LEVEL meshes
were not in scope. Marking as known-open rather than new regressions.

### Fix items

| # | Item | Effort |
|---|---|---|
| 2.1 | Chunk nodes to identity `[0,0,0]` in gen_soulslike.py:270 + gen_custom.py:80 (verts already world-space); regression-assert via mesh centroid == node-relative origin | [S] |
| 2.2 | gen_archetype: build corridor + room level meshes pivot-relative (platformer25d `track_x` pattern) or drop their node offsets to identity | [S] |
| 2.3 | lint.py: cheap origin guard — parse OBJ vertex centroid per non-terrain model, warn when a scene node referencing it has |pos.xz| > tol while the OBJ is not world-scale (catches this class statically) | [M] |
| 2.4 | Promote kit meshes to `enforce_origin=True` cleanliness so consumers stop needing auto_recenter (pairs with 1.1) | [S] |

---

## 3. Principle 3 — Terrain-conditioned placement — RATING 2/5

### Current state

* `Placement` AABB registry is solid and deterministic (worldkit.py:94–211):
  GROUND/SOLID semantics, insertion-order iteration, clone-for-trial;
  `reserve_spot` = query-before-insert with ground snap (:217–240);
  write_scene hard-validates whole batches when handed a registry (:551–585).
* Used well: gen_soulslike layout routes all scatter through reserve_spot
  (:139–204); gen_space registers station exclusion + gate + asteroids + pods
  with bounded re-rolls (:100–106, :144–151, :180–218) and passes
  `placement=reg` to write_scene (:220–221).
* Height conditioning exists ONLY as ad-hoc local math: soulslike samples its
  own fBm `height()` per node (:299–301). Works, but nothing shared.
* **`worldkit.query_height` does not exist** (repo grep). CDR-008 names it as
  the NEXT-stage substrate; today `ground_y` knows bbox tops only, never the
  fBm field that produced the terrain.

### Gaps

* gen_tabletop: pawns placed at y=0 (:104) while goal banners and gems snap
  to tile-top heights (`gt[5]`, :130, :148) — pawn bases bury up to 0.36 m
  into mountain tiles. The height data is sitting in `board_tiles` unused.
* gen_space: void plane top is y=-1.8 (cyl y0=-2 h=0.2, :127) but pods and
  the jump gate sit at y=0 (:208–211, :165) — 1.8 m hover. (Space gets
  partial amnesty: asteroids legitimately fly.)
* gen_archetype: no registry anywhere; scatter avoids path corridors by
  projection test (:167–179) but nothing rejects overlaps; all y hardwired 0.
* gen_platformer25d: manual x-spacing check for platforms (:106) — no
  registry, defensible for a linear flat level but inconsistent.
* gen_soulslike calls write_scene WITHOUT `placement=reg` (:302), skipping
  the free batch validation it already populated.
* Nothing prevents floating props in general: ground_y defaults to 0.0 when
  no walkable surface covers (x,z) — silent hover instead of error.

### Fix items

| # | Item | Effort |
|---|---|---|
| 3.1 | worldkit.`query_height(x, z)` — register chunk/terrain height functions (or sampled grids) alongside footprints; ground_y falls back to it; generators stop re-deriving fBm locally. This is CDR-008's stated NEXT substrate | [S] |
| 3.2 | Tabletop: pawns + dice to tile-top y (data already in `board_tiles`); ideally reserve_spot with per-tile tops registered as GROUND | [S] |
| 3.3 | Space: snap pods/gate to plane top (-1.8) or lift the plane to y=0; document the choice in state | [S] |
| 3.4 | Soulslike: pass `placement=reg` to write_scene (:302); archetype: adopt registry for extras/scatter with footprint tuples | [M] |
| 3.5 | ground_y: make `default=` explicit-or-error mode so callers must acknowledge unknown ground (kill silent hover) | [S] |

---

## 4. Principle 4 — Procedural materials — RATING 2/5

### Current state (end-to-end trace)

1. **Authoring** — healthy-ish. gen_archetype's `mat_at(mats, key, fallback)`
   (:63–64) maps theme-palette keys → material names per part
   (theme palette loaded from themes.json :671–677); gen_props merges
   `prop_*` palette entries into existing MTLs without recoloring
   (:231–238); soulslike replicates the merge (:247–253). No binary texture
   dependency anywhere — doctrine-compliant on that clause.
2. **MTL emission** — `write_mtl` emits Ka/Kd/Ks/Ns only (worldkit.py:447–455).
   **No Ke/emission is ever written**, despite glow/ember/star materials
   begging for it.
3. **Render consumption — NONE.** littview loads OBJs via `lv_obj_load`
   (littview.cpp:239); the parser treats `usemtl` as a mesh-flush marker and
   reads no material colors (littcore/litt_obj.c:131–140). Color comes from a
   TAG-FAMILY TINT TABLE — enemy red, hazard orange, checkpoint gold,
   pickup gold, goal green, else grey (littview.cpp:197–208) — multiplied by
   a per-part height-band shade (:275–280). Every palette, every mat_at
   lookup, every prop_* entry is invisible in renders.
4. **Engine JSON path — dead code.** litt_world.cpp parses rich materials
   (hex base_color, roughness, metallic, **emissive** :172–192) from a node
   `"material"` field (:216–228) — but write_scene never emits a material
   field, so every node resolves to "default". A receiver with no senders.
5. Consequence for the critic: native_proof's `min_colors ≥ 8` measures tag
   diversity + shading bands, NOT palette richness. Two different themes
   produce nearly identical pixels apart from geometry.

### What principle 4 needs next (in order)

1. Make Kd reach pixels: teach lv_obj_load/littview to read the MTL
   (mtllib+Kd per group) and use it as albedo, falling back to the tag tint
   when absent. Smallest change that makes the entire authored layer real.
2. Ke emission: write_mtl gains Ke for glow-class materials; littview adds an
   additive term. Bonfire/estus/stars stop rendering as flat tint.
3. Seeded generative variation: per-instance hue/value jitter derived from
   the generator's Rng (needs 1.2's --seed plumbing). Today ZERO variation
   exists anywhere — same seed or not, palettes are constants.
4. Extend emit_chunk's band_fn idea (slope/height material selection,
   worldkit.py:457–481) from terrain to props (e.g. snow caps, waterlines).
5. Only then consider map_Kd textures (parser support already exists in
   litt_obj_cpp.h:412; budget <256 KB per asset_guidelines).

### Fix items

| # | Item | Effort |
|---|---|---|
| 4.1 | littview: parse MTL Kd per usemtl group → albedo (tag-tint fallback) | [M] |
| 4.2 | write_mtl Ke + littview additive emissive term | [S] |
| 4.3 | Deterministic per-instance material variation (Rng-driven jitter at node level; needs kit --seed) | [M] |
| 4.4 | write_scene optional per-node `"material"` field → lights up litt_world.cpp's existing parser | [S] |

---

## 5. Principle 5 — Render-based refinement — RATING 2/5

### Current state

* The critic exists and is honest: native_proof.py renders each shipped game
  via `littview render` (:90), computes fill %, /24-bucket color families and
  vertical span (bmp_stats :30–49), gates fill ≥ 1.5 % and colors ≥ 8 by
  default (:54–57, :98–99), prints per-game PASS/FAIL and exits 1 if any game
  failed (:101–115). bmp_stats is already a reusable module-level function.
* lint.py (template/tools/assets/lint.py) stays deliberately structural:
  bare-usemtl, face-index overflow, dup ids, next_id, dangling model: refs
  (:9–57) + solid_count (:75–85). No visual assertions — correct division of
  labor, but nothing bridges structure→pixels except native_proof.

### Gaps — the loop is open

* **Can it FAIL generation? No — it can only fail a project after the fact.**
  make_game never invokes native_proof (audit item 8 still open; WORLDGEN_AUDIT
  §3 notes it is called only by tools/litt.py and the studio). A generator can
  ship pixel-blind output and the human discovers it by hand-running the
  script.
* Report-only: a FAIL triggers no regeneration, no re-seed, no per-asset
  diagnosis (which model underfilled the frame?), and gates are global-frame
  statistics — "assets that fail visual gates get regenerated" is unimplemented.
* Soulslike-style defects (terrain 2× displacement) pass every current gate:
  fill/colors/span are insensitive to displacement. The critic needs at least
  one geometric assertion (e.g. horizon/silhouette stability across yaw
  pairs, or bbox-aspect checks) to catch transform bugs.

### Where the regenerate-hook lives

Concretely: **in make_game.py between native validation (step 9) and deploy
(step 10)** —
1. Factor `proof_one_game(dir, min_fill, min_colors)` out of native_proof's
   loop (bmp_stats already importable).
2. Wrap the generate→props→enrich chain in
   `for attempt in range(N): build(seed_k); if proof_one_game(...): break`
   — bounded seeded retries, last frame + stats into the machine JSON, final
   failure ⇒ ok:false and no deploy.
Long-term this wrapper is literally CDR-008 stage 3's "generate → render →
assert → regenerate" agent; keeping it a pure function now costs nothing later.

### Fix items

| # | Item | Effort |
|---|---|---|
| 5.1 | Factor proof_one_game(); call it in make_game pre-deploy; fill/colors into final JSON; fail the build on miss | [S] |
| 5.2 | Bounded regenerate loop around the build chain (seeded retries, attempts logged in NOTES/LIVE_LOG) | [L] |
| 5.3 | Per-asset attribution: render with one node hidden (visible:false toggle) diff-style, or per-tag fill contribution, so "which asset failed" is answerable | [M] |
| 5.4 | Add a displacement-sensitive assertion (two-yaw silhouette delta or expected-bbox check) to catch double-transform class at the pixel level | [M] |

---

## Direct answers

**A) Does gen_soulslike use gen_props kits?** YES — item 14's compromise
landed as designed. It imports `build_prop/PALETTES/parse_mtl` (:47) and
emits the five gameplay-critical souls props (bonfire, stalker, knight,
estus_flask, banner) from the kit (:281–284). Scene dressing the kit lacks
(fog gate, bloodstain, graves, dead trees, ruin arch/pillars, soul embers)
stays bespoke in-file (:89–133) — documented, bounded debt until kit v2
absorbs those shapes (fix 1.1). Caveat: kit props go through
`auto_recenter`, not `enforce_origin`, because kit meshes miss the 0.05 m
tolerance (e.g. bonfire z=-0.06) — the kit itself isn't convention-clean yet.

**B) Any texture/material pipeline beyond flat colors?** NO. Zero textures in
the worldgen path (gen_texture.py feeds heightfields, not model materials);
flat Kd MTLs that the renderer never reads (tag-tint table wins); no
emission channel written or rendered; the engine's JSON material parser
(base_color/emissive) has no data source. Principle 4's next rungs, in
order: Kd→albedo in littview (4.1), Ke glow (4.2), seeded per-instance
variation (4.3 + 1.2), band_fn-style material selection for props (4.4),
then map_Kd.

**C) Can the critic fail generation on pixels?** It can fail a PROJECT
(exit 1) — but nothing that generates is listening. Not wired into make_game,
report-only, globally-scoped gates, no regeneration. Hook location and shape:
`proof_one_game()` called pre-deploy inside a bounded seeded retry loop in
make_game (detail in §5). Displacement-class bugs additionally need a
geometric pixel assertion (5.4) — today's fill/colors gates are blind to the
worst extant bug.

**D) Furthest flagship from the doctrine?** **gen_archetype** — the workhorse
every make_game game actually ships on. It fails P1 outright (zero kit usage,
duplicate bespoke vocabulary), carries two live double-transformed level
meshes (corridor_run, room_graph) against P2, has no P3 machinery at all (no
registry, no height query, overlap-blind scatter), and its genuinely good P4
authoring (mat_at + themes.json) is erased by the renderer. Irony: it has the
best material *source convention* in the repo and the least material *effect*.
Runner-up: gen_tabletop (kit-less, pawn burial, no registry). Closest:
gen_soulslike, dragged down mainly by the inherited chunk-placement bug.

---

## TOP-10 PUNCH LIST (priority order)

1. **[S]** Kill the terrain-chunk double-transform: chunk nodes to identity in
   gen_soulslike.py:270 + gen_custom.py:80 (P2 — active corruption, cheapest fix)
2. **[S]** gen_archetype corridor_run + room_graph level meshes: pivot-relative
   or identity nodes (P2 — second active corruption, same class)
3. **[S]** Wire the critic: proof_one_game() in make_game pre-deploy, pixel
   metrics in final JSON, hard-fail on miss (P5 — makes the doctrine enforceable)
4. **[M]** littview reads MTL Kd as albedo, tag-tint fallback (P4 keystone —
   without it principles 4 AND 5 are judging imaginary materials)
5. **[S]** Tabletop grounding: pawns/dice to tile-top heights; register tiles
   as GROUND in a Placement (P3 — visible clipping bug)
6. **[S]** worldkit.query_height substrate: registered heightfields behind
   ground_y; delete per-generator fBm re-derivations (P3 — CDR-008 NEXT item)
7. **[S]** Ke emission channel end-to-end: write_mtl + littview additive term
   (P4 — glow props finally glow; cheap)
8. **[M]** gen_props v2: absorb common bespoke meshes, add --seed variation,
   themes.json palettes, enforce_origin-clean meshes (P1+P2+P4 foundation)
9. **[M]** Migrate gen_archetype extras + tabletop pieces onto kit refs; adopt
   registry in archetype scatter (P1+P3 — ends the vocabulary split)
10. **[L]** Closed regenerate loop in make_game: generate→render→assert→
    re-seed, bounded attempts, attempts logged (P5 capstone — becomes the
    CDR-008 stage-3 critic agent)

*Deferred/watch:* space pods/gate hover fix (fold into 5); silent-hover
ground_y error mode; per-asset pixel attribution; displacement-sensitive
pixel assertion; soulslike write_scene placement=reg pass-through.

## RESOLVED (python wave)

All fixes below landed in template/tools/worldgen/ + template/tools/assets/lint.py
only; every gate re-run green (py_compile x8, test_worldkit 15/15 incl. new
query_height tests, littcli validate ok:true on all five probes, SHA256
determinism match, lint clean + synthetic warning proof).

* **2.1** Chunk double-transform killed at both call sites (gen_soulslike.py,
  gen_custom.py): chunk nodes now `[0,0,0]` AND `world_state.json`
  `chunks[].position` now `[0,0,0]` — play_native.py:126 offsets chunk meshes
  by state positions too, so leaving state offset would have kept the ~2x
  sim/render corruption alive through a second path. Tags/model refs untouched.
* **2.2** gen_archetype corridor_run + room_graph rebuilt PIVOT-RELATIVE
  (platformer25d track_x pattern; identity-node option rejected in-code):
  level mesh translated so its vertex centroid sits at origin, node carries
  the pre-shift centroid as placement. Probe numbers — Corridor node
  [36.737, 0, 0.554], OBJ centroid xz-dist 0.0000; Dungeon node
  [19.647, 0, 16.324], centroid xz-dist 0.0001. Extras untouched.
* **3.1** `Placement.register_height_field(name, fn_or_grid, bounds)` +
  `query_height(x, z)` shipped: fn or >=2x2 grid (emit_chunk lattice,
  bilinear), last-registration-wins override, None outside all regions,
  ValueError on non-finite; ground_y falls back surface -> field -> default;
  clone() carries fields. gen_soulslike fBm sampling migrated onto it.
* **3.2** gen_tabletop pawns + dice grounded on board_tiles tile-top heights
  (`tile_top_at` nearest-tile helper); e.g. Pawn_01 y=0.240 == mountain-band
  tile top 0.240 — no more 0.36 m burial.
* **3.3** gen_space CHOICE: void-plane TOP lifted to y=0 (cyl y0=-0.2 h=0.2);
  pods/gate keep y=0 nodes; documented in `state.gameplay.note`.
* **2.3** lint.py `lint_double_transform()` wired into lint_game problems:
  warns when a non-terrain node has |pos.xz| > 0.5 while its model's OBJ
  vertex centroid is > 1.5 off origin (xz-plane distance, matching the
  base-center convention). Terrain tag = sanctioned exception (proved by
  offset-chunk synthetic staying silent); fires on a real bad case
  (Pawn_01 +20 m node + baked mesh).
* **3.4 partial** gen_soulslike passes `placement=reg` to write_scene with
  REAL footprints (put() now emits half-extents matching reserve_spot's
  inserts, so validation is idempotent-but-meaningful, not vacuous).

— Python wave complete; native-side items (4.1, 4.2, 5.x) remain open.

— Audit complete. Evidence lines current as of this session; generators
mid-flight (items 3–6) were audited as-found with in-flight notes.
