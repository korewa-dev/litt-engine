# WAVE: make_game flagship dispatch + native proof gate + collision placement

Scope: `template/tools/worldgen/make_game.py` ONLY. Closes WORLDGEN_AUDIT
punch items 7, 8, 9. Generators, worldkit.py, native_proof.py untouched.

## Item 7 [M] - Flagship dispatch

New `--kind {soulslike,space,tabletop,platformer25d,archetype}` (default
`archetype` = byte-for-byte today's pipeline). Flagship kinds run their
dedicated generator with the user's seed/out-dir/agent/prompt:

| kind          | generator            | default kit |
|---------------|----------------------|-------------|
| soulslike     | gen_soulslike.py     | souls       |
| space         | gen_space.py         | survivor    |
| tabletop      | gen_tabletop.py      | survivor    |
| platformer25d | gen_platformer25d.py | platformer  |
| archetype     | gen_archetype.py     | intent map  |

Post-steps unchanged and shared by every kind: gen_props(kit) -> scene_layout
-> brief_for -> gen_story -> story merge -> enrich_game -> lint_game.
`--kind` alone satisfies the arg guard; legacy `--archetype platformer25d`
alias preserved verbatim.

## Item 8 [S] - Native proof gate

After generation+lint (`native_proof_gate()`): for the produced game dir,

1. `littcli validate --frames 120` must parse ok:true AND interactives>0 AND
   missing==0 (reuses the step-9 validate result; runs its own only on the
   legacy `--skip-validate` path).
2. `littview render <dir> --out %TEMP%\littproof_<name>.bmp` + `bmp_stats`
   (imported from template/tools/assets/native_proof.py) must show
   fill >= 1.5% AND colors >= 8.

Every failed assertion is printed by name (`[make] native-proof FAIL:
expected ...`) then a JSON failure line; exit 1. PASS line prints solids/
interactives/missing/fill/colors. Stats flow into NOTES.md and the final
machine JSON as `"native_proof": {...}`. `--skip-native-proof` skips the
whole gate (prints SKIPPED).

## Item 9 [M] - Collision-aware placement hook

`ExtraPlacer` wraps make_game's own extras scatter (story items + roster
enemies in the story-merge loops). worldkit.Placement registry: layout union
bbox registered as GROUND, authored brief anchors (spawn/checkpoints/nodes)
as SOLID boxes (0.6 m half-extent); each candidate re-rolls along the spine
plus across the short axis (bounded 10 seeded attempts, clamped to the
walkable span) then is DROPPED with a log line naming the blockers.
Deterministic: `random.Random("extras/<seed>")`, fixed draw order.

## Gates (all pass)

- `python -m py_compile template/tools/worldgen/make_game.py` -> exit 0.
- Five E2E probes into %TEMP% (seeds 7/11/22/33/44), each exit 0, lint clean,
  gate PASS (see session log for verbatim lines):
  archetype fill=4.18% colors=40 inter=42 | soulslike fill=19.98% colors=35
  inter=43 | space fill=81.16% colors=35 inter=63 | tabletop fill=7.92%
  colors=50 inter=44 | platformer25d fill=5.18% colors=31 inter=53.
- Regression: default invocation without --kind (--random --seed 7) exit 0,
  gate PASS - pipeline unchanged apart from the added gate.
- `--skip-native-proof`: no littcli/render output, ~1.5 s total run.
- Determinism: two same-seed/same-name space builds -> all 32 files
  byte-identical except worldkit's own wall-clock fields ("updated" stamp in
  world_state.json; LIVE_LOG minute stamps) written by write_state/log inside
  generators, which this wave may not modify.
- All probe dirs removed; Project/games.json snapshotted before probes and
  restored after; repo diff = make_game.py only.
