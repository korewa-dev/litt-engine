# WAVE 1 — WorldKit Placement Registry + Transform Convention

Implements punch-list items **1** and **2** from `docs/reports/WORLDGEN_AUDIT.md`.
Files touched: `template/tools/worldgen/worldkit.py` only (+ new test file
`template/tools/worldgen/test_worldkit.py`). No generator callers were rewritten;
the capabilities are opt-in so every existing generator keeps its exact behavior.

---

## Task A — AABB placement registry

New section in `worldkit.py`. Rejection policy (documented choice): **`insert()`
returns `False` on conflict and never mutates state**; use `conflicts()` to learn
who blocks. Edge-touching boxes count as overlapping. Iteration is always
insertion order (plain dicts, zero sets) → hash-randomization can never change
output bytes.

### `class Placement`
| Member | Signature | Notes |
|---|---|---|
| `insert` | `(name, min_xy, max_xy, top=0.0, walkable=False, blocks=None) -> bool` | `False` on overlap or duplicate name (no mutation). Reversed corners normalized; non-finite values raise `ValueError`. |
| `conflicts` | `(min_xy, max_xy, ignore=()) -> list[str]` | Blocking entries overlapping the query box, insertion order. |
| `ground_y` | `(x, z, default=0.0) -> float` | Ground-snap: bbox-top Y of highest **walkable** surface covering `(x,z)`; `default` if none. |
| `contains` | `(x, z) -> bool` | Any walkable surface covers the point. |
| `bounds` | `(name) -> ((min_x,min_z),(max_x,max_z), top, walkable)` | Snapshot. |
| `names` / `__len__` / `__iter__` / `__contains__` | — | Insertion-order guaranteed. |
| `clone` | `() -> Placement` | Faithful deterministic copy (used for trial validation). |

**Ground vs solid semantics** (added after tests exposed that a big walkable
floor would otherwise block every prop placed on it):
- `walkable=True` → GROUND: provides top Y to `ground_y`, never blocks overlaps.
- `walkable=False` (default) → SOLID obstacle: occupies space, rejects overlaps.
- `walkable=True, blocks=True` → standable platform that does both.
Practical pattern: register terrain/floors/decks as ground; register walls/rocks
as solids; props default to solid.

### Module helpers
- `center_box(cx, cz, w, d) -> (min_xy, max_xy)` — center + FULL width/depth.
- `reserve_spot(registry, name, cx, cz, w, d, lift=0.0, top=None,
  walkable=False, blocks=None, y_default=0.0) -> [x, y, z] | None` —
  collision-safe placement sugar: queries ground + conflicts BEFORE reserving;
  returns scene-node position with `y = ground_y(x,z) + lift`, or `None` when
  blocked; commits the footprint on success.

### Threaded through the writers (opt-in)
- `save_prop(..., enforce_origin=False, auto_recenter=False, origin_axes="xz",
  origin_tol=ORIGIN_TOL)` — convention hooks (see Task B); legacy calls unchanged.
- `write_scene(path, placed, title, placement=None)` — items may carry an
  optional 6th element `footprint = (half_w, half_d)`. With `placement=reg`,
  every footprinted node is validated against the registry AND against earlier
  nodes of the same batch **before anything is written**: any conflict raises
  `ValueError` and the scene on disk plus the caller's registry stay untouched;
  a clean batch commits all footprints into the registry. Idempotent by name:
  a node pre-tracked via `reserve_spot()` passes iff its x/z bounds are
  unchanged; moved/duplicated names raise instead of double-booking.
  Bonus hardening: falsy `model_ref` (5th element) now falls back to the
  snake-cased name instead of crashing on `"model:" + None`.

## Task B — transform convention enforcement

Convention documented at the top of `worldkit.py`: **props are modeled AT
ORIGIN (footprint centered x=z=0, base at y=0) and positioned ONLY via scene
node position/yaw.** Baking world coords into vertices AND setting node
position displaces such props ~2x under transform-applying consumers.
Sole sanctioned exception: `emit_chunk()` terrain bakes WORLD vertices by
design (seamless chunking) and must sit at node position `[0, *, 0]` (noted in
its docstring).

- `TransformError(ValueError)` — violation type.
- `mesh_centroid(mesh) -> [cx, cy, cz]` — accepts MeshBuilder or raw vertex rows.
- `assert_origin_centered(mesh, tol=ORIGIN_TOL, axes="xz") -> True` — raises
  `TransformError` listing offending axis deltas. Default `axes="xz"` matches
  base-center convention (y may ride up the mesh); `"xyz"` available.
- `recenter_mesh(mb, axes="xz") -> (dx, dy, dz)` — repair helper.
- `MeshBuilder.translate(dx, dy, dz)` — deterministic shift, chainable.
- `ORIGIN_TOL = 0.05` module constant.

**Latent bug found & fixed while wiring this:** `MeshBuilder._vi` used to append
the caller's point *list object*, and primitives share corner objects across
faces — any vertex translation was applied once per reference (a recentered
cube landed at nonsense coords). `_vi` now copies points; values and therefore
existing OBJ bytes are unchanged for all generators.

## Verification (all commands run from repo root)

1. `python -m py_compile template/tools/worldgen/worldkit.py` → OK (test file too).
2. `python template/tools/worldgen/test_worldkit.py` → **ALL 12 TESTS PASSED**
   (Rng determinism; insert/conflicts/ground_y/contains/iteration order;
   reserve_spot incl. blocked + stacking; centroid/assert/recenter incl. axes
   variants; save_prop enforce/auto-recenter/mutual-exclusion; write_scene
   backward-compat tags, placement conflicts w/ atomicity, batch overlaps,
   duplicate names, idempotent rewrite; byte-determinism of mesh text and
   whole scenes).
3. Regression probe (twice):
   `python template/tools/worldgen/gen_archetype.py --archetype soulslike --pattern corridor_run --theme dark_fantasy --seed 4242 --name wk-regress --out-dir $env:TEMP/litt-wk-regress`
   → both runs exit 0, "3 assets, 12 nodes"; coin/goal_banner/layout_main OBJs,
   materials.mtl, world.lscn.json, asset_index.json all **SHA256-identical
   across the two runs** (same-seed ⇒ byte-identical holds end-to-end).
   `lint_game()` on the output: `{'objs': 3, 'problems': [], 'dangling_refs': []}`.
   Both temp dirs deleted afterwards.

## Notes for downstream punch-list items
- Item 3/16/18 (gameplay wiring): use `reserve_spot` + footprinted tuples so
  pickups/enemies get collision-safe spots and ground snap for free.
- Items 6/17/20 (double-transform fixes): rebuild meshes around origin, then
  either call `assert_origin_centered` or `save_prop(..., enforce_origin=True)`;
  `recenter_mesh` repairs already-baked meshes without touching base height.
- Item 7 (make_game place_on): `Placement.ground_y` + padded boxes replace
  spine-fraction math; `clone()` enables dry-run validation.
- Determinism rules respected throughout: no `random.Random`, no sets, no
  nondeterministic iteration anywhere in the new code.
