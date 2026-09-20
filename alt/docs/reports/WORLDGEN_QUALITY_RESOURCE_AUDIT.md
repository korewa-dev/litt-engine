# WorldGen Quality and Resource Audit

Date: 2026-09-20
Task: #48

## Gate added

`worldgen_quality_gate.py` now generates representative outputs for all six parametric archetype patterns plus the soulslike, space, tabletop, and 2.5D platformer flagship generators.

The gate checks:

- repeat generation with the same seed is deterministic after excluding intentionally volatile log/timestamp fields
- generation completes within 8 seconds per representative world
- output remains below 3 MiB
- file/model/unique-model counts remain bounded
- scenes are non-empty
- metrics expose node count and model-instancing count for regression review

It runs inside the generated-game CI job.

## Measured representative results

| case | generation | output | files | OBJ models | scene nodes | model instances |
|---|---:|---:|---:|---:|---:|---:|
| arena | 0.054 s | 147.8 KB | 9 | 4 | 8 | 7 |
| corridor | 0.051 s | 52.7 KB | 8 | 3 | 13 | 12 |
| hub | 0.069 s | 217.7 KB | 15 | 10 | 27 | 26 |
| board | 0.064 s | 219.2 KB | 8 | 3 | 8 | 7 |
| track | 0.055 s | 125.9 KB | 8 | 3 | 4 | 3 |
| rooms | 0.049 s | 66.0 KB | 8 | 3 | 5 | 4 |
| soulslike | 0.293 s | 1956.4 KB | 52 | 47 | 79 | 78 |
| space | 0.051 s | 222.9 KB | 14 | 9 | 314 | 313 |
| tabletop | 0.063 s | 277.4 KB | 16 | 11 | 16 | 15 |
| platformer25d | 0.046 s | 70.6 KB | 15 | 10 | 35 | 34 |

All representative cases were deterministic.

## Resource repair

The soulslike generator was the clear outlier at roughly 5.5 MiB and 0.6 s before this pass. Its 35 terrain chunks each used a 24x24 cell grid. The default terrain resolution is now 12x12 cells per chunk.

This preserves:

- the same world footprint
- the same deterministic fBm height function
- the same chunking/streaming boundaries
- the same placement/collision height source
- the same gameplay layout

while cutting terrain cell/triangle density to roughly one quarter.

Measured representative soulslike output fell from about 5529.9 KB to 1956.4 KB and generation time from about 0.616 s to 0.293 s on GitHub's Ubuntu runner.

## Remaining interpretation

The space generator deliberately has a high node-to-model ratio: 313 model instances from only 9 OBJ models. This is consistent with Litt's low-resource philosophy because it reuses geometry rather than emitting hundreds of unique mesh files.

The soulslike generator still has 47 OBJ files because 35 are terrain chunks. Its byte budget is now acceptable, but a future runtime with true terrain streaming/heightfield rendering could reduce file count further without sacrificing terrain extent.

## Release conclusion

Representative WorldGen output now has a permanent deterministic/resource regression gate. The current samples pass the 3 MiB per-world output budget and generate well below one second on the CI runner.

This closes the immediate WorldGen release-readiness resource issue while keeping future regressions measurable.
