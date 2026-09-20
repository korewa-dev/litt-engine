# Litt Engine Release Readiness Roadmap

Goal: make the supported Litt path dependable, small, reproducible, portable, and useful before expanding the GUI.

Philosophy: little runtime cost, little dependency burden, large useful capability.

## Definition of mostly ready

The supported path must build/run from a clean checkout on Windows and Linux, pass sanitizer/native/generated-game/public-header gates, have no known critical/high correctness blockers, expose one documented supported runtime/API path, generate and run a representative game deterministically, have measured resource budgets, and clearly label experimental systems.

## Mandatory loop

Every subsystem goes through:

1. **CHECK**: implementation, API, portability, safety, resource use, tests, docs, integration.
2. **FIX**: correctness, unsafe behavior, portability, stale interfaces, avoidable resource costs.
3. **VALIDATE**: regressions and edge cases.
4. **IMPLEMENT**: missing functionality required by the supported Litt experience.
5. **COMPILE**: clean-build public headers, engine, tools, tests, and generated games.
6. **BENCHMARK**: binary size, startup, memory where practical, generation/output cost, mesh/entity counts, runtime cost.
7. **CONSOLIDATE**: obsolete/duplicate paths, naming, docs, stable API.
8. **RELEASE GATE**: complete clean matrix.
9. **MERGE**: only after the gate is green.

## Work order

| # | Area | State |
|---|---|---|
| 1 | Build / CI / portability | ACTIVE |
| 2 | Core runtime / public API | TODO |
| 3 | Math / memory | TODO |
| 4 | ECS / events / input | TODO |
| 5 | Assets / OBJ / materials | TODO |
| 6 | Rendering / viewer | TODO |
| 7 | Physics / collision / spatial | TODO |
| 8 | Audio | TODO |
| 9 | Serialization / networking / scripting | TODO |
| 10 | WorldGen | TODO |
| 11 | Generated-game pipeline | TODO |
| 12 | Tools / examples | TODO |
| 13 | Docs / naming / packaging | TODO |
| 14 | Final clean release matrix | TODO |
| 15 | Litt GUI integration | WAIT: ENGINE FIRST |

## Current gate

Historical audits are evidence, not release proof. Executable gates are authoritative. The readiness branch is repairing failures exposed by generated-game tests and the Windows standalone-header sweep. Build/CI is not complete until every Stabilization job passes on the same revision.

## WorldGen quality gate

For representative generators measure determinism, generation time, file count/disk footprint, unique vs reusable meshes, entity/collision/gameplay-node counts, navigability/connectivity, transform correctness, placement validity, gameplay/visual variety, and runtime cost where measurable.

Prefer a smaller reusable asset set with better gameplay structure over hundreds of mediocre unique meshes.

## Resource gate

Capture baselines where practical for binary size, startup, idle/runtime memory, major fixed allocations, frame/render cost, generated-world disk size, generation time, and dependency footprint. Resource efficiency is a Litt release requirement.

## Merge policy

Critical/high correctness or build blockers must be fixed. Medium supported-path defects must be fixed or explicitly justified and tracked. Experimental systems may be isolated and labeled when not required by the supported path. Documentation must not advertise experimental paths as production-ready. A green platform cannot override a red supported-platform gate.

## GUI handoff

Start Litt GUI repair only after the engine has a stable supported contract. Then apply the same CHECK -> FIX -> VALIDATE -> IMPLEMENT -> COMPILE -> BENCHMARK -> CONSOLIDATE -> RELEASE GATE loop to the GUI.
