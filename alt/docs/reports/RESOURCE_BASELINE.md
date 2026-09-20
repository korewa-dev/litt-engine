# Litt Engine Resource Baseline

Date: 2026-09-20
Release task: #49

Litt's supported path is intentionally dependency-light. Resource regressions are release concerns, not cosmetic issues.

## Enforced WorldGen budgets

The generated-game CI job executes `worldgen_quality_gate.py`. Representative generated worlds must satisfy:

- output size <= 3 MiB
- files <= 160
- OBJ models <= 56
- unique referenced models <= 48
- scene nodes <= 512
- model instances <= 512
- generation time <= 8 seconds
- same seed produces the same canonical output

Current measured representative outputs range from about 52.7 KiB to 1.96 MiB and from 0.046 s to 0.293 s on the GitHub Ubuntu runner. The largest node count is the space sample at 314 nodes, backed by only 9 OBJ models, demonstrating geometry reuse rather than unique-mesh proliferation.

The soulslike sample was reduced from about 5.53 MiB / 0.616 s to about 1.96 MiB / 0.293 s by reducing terrain cells per chunk from 24x24 to 12x12 while preserving the world footprint and deterministic height source.

## Native dependency baseline

The release-supported native runtime is built directly from:

- `litt_json.c`
- `litt_obj.c`
- `litt_world.c`
- `littcli.c` or `littview.cpp`

The POSIX Makefile links the standard math library only. The supported build does not require Vulkan, DX12, Python embedding, C#, Lua, networking libraries, or an editor framework.

## Binary and runtime measurement

Binary size and process-memory figures are platform/toolchain dependent and should not be frozen to a misleading universal number. CI instead protects the primary structural causes of resource growth: dependency additions and generated content/file/node/model budgets.

When making a release build, record `bin/littcli` and `bin/littview` file sizes for the target toolchain. A material unexplained increase should block release review.

## Fixed-allocation rule

Release-supported code must not introduce large fixed buffers proportional to maximum hypothetical resolution/world size when bounded dynamic allocation or streaming is practical. Experimental/editor integrations are outside the supported contract until independently measured and gated.

## Regression policy

Changes that exceed an enforced budget must either reduce the resource cost or deliberately revise this baseline with measured evidence and a rationale. Silently weakening the gate is not an acceptable fix.
