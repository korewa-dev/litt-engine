# WAVE_SPACE — punch-list item 15 (gen_space OBJ explosion)

## Change

`template/tools/worldgen/gen_space.py` only. Stars collapse from one OBJ per
star to ONE shared `star.obj` mesh + N `Star_*` instance nodes tagged
`model:star` (hub_spoke coin pattern from gen_archetype.py: extras emit one
mesh, then `(None, ref, name, None, pos, yaw, tags+["model:"+ref])` nodes;
worldkit.write_scene honors explicit `model:` tags, worldkit.py:512).
Asteroids: `AST_VARIANTS = 4` seeded variant meshes (`asteroid_v01..04`,
same p_asteroid rng math) + one hazard node per asteroid cycling variants.
Pods: one `escape_pod.obj` + N nodes, gameplay tags unchanged
(`objective,salvage`). Station/void plane untouched. Uniform ±90/±40 scatter
kept as-is (collision-safe scatter is item 16).

Determinism: same Rng(seed) protocol; fixed draw order (4 variant builds →
per-star xyz → per-asteroid xyz+yaw → per-pod xz+yaw), round(...,2) as before.
Note: star color variety (white/blue/gold pick) was removed with the single
mesh — star_blue survives on the pod port; restore later only as extra star
variant meshes if ever wanted (would add 2 OBJs, still ≤12).

## Gates (verbatim)

```
python -m py_compile template/tools/worldgen/gen_space.py   → exit 0
python template/tools/worldgen/gen_space.py --out-dir %TEMP%/litt-space-smoke --agent wave-space
  → "[voiddrift] ready: 8 assets, 310 scene nodes", exit 0
OBJs in assets/models → 8 (<=12): asteroid_v01..04, derelict_station,
  escape_pod, star, void_plane
star nodes in world.lscn.json → 280 (>=200); total placed nodes 310 (+1 root)
lint_game(probe) → {"objs": 8, "problems": [], "dangling_refs": []}
run 2 vs run 1: asset_index.json BYTE-IDENTICAL, world.lscn.json
  BYTE-IDENTICAL, all 8 OBJs byte-identical (world_state.json differs only by
  its designed `updated` timestamp)
native/bin/littcli.exe validate <probe> --frames 120 →
  {"ok":true,"mode":"Orbit3D","frames":120,"solids":2,"interactives":29,"tris":200,"missing":0}
```

Before → after: ~310 OBJs (280 stars + 24 asteroids + 4 pods + station +
void plane) → **8 OBJs**, flat regardless of `--stars`. All %TEMP%
litt-space-smoke* probe dirs removed after verification.
