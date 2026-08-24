# WAVE_SOULS_1314 — gen_soulslike.py seed plumbing + kit composition

Scope kept: only `template/tools/worldgen/gen_soulslike.py` touched
(worldkit.py / registry integration left to owning waves).

## Item 13 — Seed plumbing [DONE]
- New `--seed N` CLI arg. Derivation (`derive_seeds`, documented in module
  docstring): splitmix64 finalizer truncated to 32 bits over two distinct
  odd constants:
  - `terrain = mix64(seed ^ 0x9E3779B97F4A7C15)` (golden-ratio gamma)
  - `scatter = mix64(seed ^ 0xD1B54A32D192ED03)` (splitmix gamma)
- `terrain` drives fBm height/band sampling of every chunk OBJ; `scatter`
  drives ONE xorshift32 `Rng` threaded through `layout(rng, reg)` and the
  prop-mesh variation builders (grave/dead_tree/pillar) in fixed draw order.
- Omitting `--seed` preserves the historical consts T666/S2077.
- Seeded variation within contract-safe bounds: hollows 3–5 spread up the
  road corridor (|x| ≤ 4.5), embers 7–8 clustered ≤3 m from landmarks,
  estus ×3 near bonfire/gate/arena, graves 8–10 and dead trees 5–7 always
  off-corridor (|x| ≥ ~3 m), counts jitter per seed.

## Item 14 — Composition rebuild [DONE, adapted]
- Adopted gen_props souls kit by IMPORT: `build_prop()` + `PALETTES`
  ["haunted_estate"] + `parse_mtl` merge convention (`prop_*` setdefault,
  never recolors ash palette). Kit meshes now used for: bonfire
  (checkpoint + player start), stalker (hollow enemies), knight (boss),
  estus_flask (pickups), banner ×4 flanking fog gate + arena.
- Kept bespoke (kit lacks them): fog gate, corpse bloodstain, graves,
  dead trees, ruin arch/pillars, soul embers — nothing overlapped the kit
  trivially, so no dedupe loss.
- Placement via worldkit.Placement/reserve_spot collision registry; story
  anchors reserved first so scatter routes around them.
- Transform convention: bespoke symmetric props saved with
  `enforce_origin=True`; imported kit meshes carry small pre-existing
  centroid offsets (e.g. bonfire z=-0.06) → saved with sanctioned
  `auto_recenter=True`; random-branch dead tree auto-centers too.
- world_state.json records `{input, terrain, scatter}` seeds; LIVE_LOG +
  stdout line report derived streams.

## Gate evidence (repo root)
```
python -m py_compile template/tools/worldgen/gen_soulslike.py   → exit 0
gen --seed 111 / 222 / 111-rerun into %TEMP%                    → all exit 0
SHA256 world.lscn.json      A≠B, A==A2                          → PASS
SHA256 chunk_0_0.obj        A≠B, A==A2                          → PASS
SHA256 asset_index.json     A==A2 byte-identical                → PASS
  (index is seed-independent by design: stores id/path/loader only;
   A vs B equality expected — seed lives in file contents)
lint_game(dir): {"objs": 0, "problems": [], "dangling_refs": []} both dirs
littcli validate A --frames 120 → {"ok":true,"mode":"Orbit3D","frames":120,
  "solids":35,"interactives":16,"tris":10080,"missing":0}
littcli validate B --frames 120 → {"ok":true,...,"solids":35,"interactives":13,"missing":0}
Tag census A(111): enemy5/boss1/pickup+souls10/checkpoint1/player+start1/
  boss_entry1/corpse_run memorial1/terrain35 → CONTRACT True
Tag census B(222): enemy4/pickup+souls8/rest same            → CONTRACT True
legacy default smoke (no --seed) → T666/S2077, exit 0
%TEMP% probe dirs: 4/4 removed, 0 remaining
```

Node totals: 81 nodes @seed 111, 76 @222 (seeded scatter retry drops),
79 legacy. Assets: 35 chunks + 47 models (was 45).
