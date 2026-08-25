# WAVE_SPACE16 — gen_space.py placement + goal semantics (audit item 16)

Scope: `template/tools/worldgen/gen_space.py` ONLY. No other generator, worldkit,
make_game, or engine file touched.

## TASK A — Collision-safe scatter
- One `worldkit.Placement` registry tracks every solid XZ footprint.
- Station reserves its exclusion box first: half-extent 4.6 m (ring r≈3.4 + margin).
- Uniform SOLID_HALF = 2.0 m footprints for asteroids / pods / gate → AABB
  rejection guarantees pairwise XZ center distance ≥ 4.0 m everywhere.
- Candidates re-roll up to PLACE_ATTEMPTS=12 times, then are skipped and the
  skip is counted in stdout + LIVE_LOG (seed 4242 probe: 0 skipped).
- Stars stay backdrop-only (y 14–60) and never enter the registry.

## TASK B — Goal semantics
- Exactly ONE goal node: `Jump_Gate` at (+46, 0, 0), origin-centered shared
  mesh (`jump_gate.obj`, enforce_origin=True), tags `["goal","poi","jump_gate"]`.
- Pods default 4 → 6; retagged `["pickup","salvage","model:escape_pod"]`
  (was `objective`) → ≥5 pickup-tagged salvage pods.
- `Derelict_Station` keeps tags `poi/salvage/level/hub`.
- `state.gameplay.objective` set to a short machine-readable line; prose
  movement/hazards replaced by structured `physics` dict (6dof_thrusters,
  gravity/run_speed readable by native litt_world.c) and hazards as node lists;
  `goals`/`pickups`/`hub` name arrays added (schema matches gen_soulslike.py /
  gen_tabletop.py gameplay blocks).
- Meshes now honor the origin convention: station/asteroid/pod use
  auto_recenter (asymmetric greebles), plane/star/gate enforce_origin.
- write_scene(..., placement=reg) hard-validates every footprinted node.

## Gates (all verbatim)
```
python -m py_compile template/tools/worldgen/gen_space.py        → exit 0
probe run --out-dir %TEMP%/litt-space16                          → exit 0, "[voiddrift] ready: 9 assets, 313 scene nodes | census: goal=1 pickup=6 hub=1 hazard=24 | skipped: ast=0 pods=0"
lint_game                                                        → {"objs": 9, "problems": [], "dangling_refs": []}
native/bin/littcli.exe validate <dir> --frames 120               → {"ok":true,"mode":"Orbit3D","frames":120,"solids":2,"interactives":32,"tris":200,"missing":0}
census                                                           → {"goal": 1, "hazard": 24, "hub": 1, "pickup": 6} stars=280 total_nodes=313
overlap 10 closest pairs                                         → min dist 4.700 vs need ≤1.946 → PASS (radii from model bounds)
SHA256 asset_index.json rerun                                    → 5fd2aa9e8b80a145 == 5fd2aa9e8b80a145 IDENTICAL
SHA256 world.lscn.json rerun                                     → e68fb22da01f4aa0 == e68fb22da01f4aa0 IDENTICAL
cleanup                                                          → %TEMP%/litt-space16 + checker tempdirs deleted
```

Determinism unchanged in kind: registry iteration is insertion order, all
randomness flows through Rng(seed); same seed → byte-identical gated JSONs.
