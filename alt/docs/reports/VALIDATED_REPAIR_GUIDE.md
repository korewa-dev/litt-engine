# Validated Repair Guide

Source audit: `alt/docs/reports/VALIDATED_HEAVY_AUDIT.md`
Audit baseline: `15235b43a32c1f11ecee828670b8a55c3d721be2`

## Non-negotiable repair rules

Use a fresh branch from current `main`.

Do not merge or cherry-pick `audit-c-bridge-components-20260922`. Its attempted C-bridge repair deletes 305 lines from `litt_c.cpp` and is not a surgical change.

Do not use `c-bridge-component-contract-20260920` as a base. It is stale and far behind current main.

For every repair:

`understand -> specify -> test -> implement -> validate -> audit -> merge -> validate main`

Do not weaken tests, capability checks, support contracts, or failure behavior to get green CI.

## Repair 1: fix canonical ray/AABB inside-origin distance

Priority: 1
Severity: RED
Expected scope:
- `alt/src/native/littcore/litt_math.h`
- a currently gated C++ contract test, preferably `alt/src/native/littcore/litt_engine_tests.cpp`

### Specify

For a finite ray interval intersecting an AABB:

- if the computed slab entry is after the ray minimum, report the entry;
- otherwise, when the ray begins inside the box, report the computed slab exit;
- if the selected intersection is outside the ray interval, report no hit.

Preserve existing strict-boundary behavior unless a separate contract change is justified.

### Test first

Add regressions equivalent to:

1. outside ray: origin `(0,0,-5)`, direction `+Z`, box z `[0,2]`, expected `t=5`;
2. inside ray: origin `(0,0,0)`, direction `+X`, box x `[-1,1]`, ray far `10`, expected `t=1`;
3. clipped inside ray: same box/origin/direction but ray far `0.5`, expected no hit;
4. parallel miss: a direction parallel to one slab with origin outside that slab, expected no hit.

Confirm the inside-origin regression fails before implementation.

### Implement

Make the smallest change in `ray_aabb`: when entry is before the permitted ray minimum, use the computed slab exit `tmax`, not `r.t_max`.

Do not refactor unrelated math code.

### Validate

Run at minimum:

```sh
make engine-contract-test
make release-hardening-test
make -C alt/src/native cpp-sdk-test
```

Then run the full Stabilization workflow through the PR and again on exact merged main.

## Repair 2: make C bridge component configuration truthful

Priority: 2
Severity: ORANGE
Expected scope:
- `alt/src/native/litt_c.h`
- `alt/src/native/litt_c.cpp`
- `alt/src/native/litt_c_tests.cpp`

### Specify before code

Document the existing minimal component contract instead of inventing new subsystem schemas:

- the C bridge currently owns component presence metadata;
- Transform data is configured through the dedicated transform API;
- non-transform typed JSON configuration is not yet a promoted contract;
- default/no configuration may be represented by null, empty string, or a strict empty JSON object;
- meaningful or malformed configuration must fail explicitly until a typed schema and backing runtime behavior are implemented.

This keeps current presence behavior but eliminates silent acceptance of ignored configuration.

### Test first

Extend `litt_c_tests.cpp` with all of the following:

- add Mesh with `nullptr` succeeds and sets presence;
- remove it;
- add Mesh with `""` succeeds and sets presence;
- remove it;
- add Mesh with `"{}"` succeeds and sets presence;
- remove it;
- add Mesh with a meaningful object such as `"{\"path\":\"mesh.obj\"}"` fails and does not set presence;
- malformed JSON fails and does not set presence;
- array/scalar configuration fails and does not set presence;
- invalid entity and invalid component type still fail;
- Transform remains present and continues to use the transform setter/getter contract.

Confirm the meaningful-config test fails before implementation.

### Implement surgically

Do not replace `litt_c.cpp`.

Add a small helper that classifies default/no configuration. Prefer the existing strict JSON parser already in the native dependency graph. If parsing a non-empty configuration:

- parse strictly;
- accept only an empty object as the current no-configuration form;
- free the parse tree on every path;
- reject everything else before mutating the component bitmap.

Keep this change local to the component-add path.

### Validate

Run the C-bridge contract locally/CI before any merge.

Also inspect the diff and reject it if unrelated functions disappear or large unrelated blocks change. The expected `litt_c.cpp` implementation diff is small.

## Repair 3: close C bridge Windows and sanitizer proof gaps

Priority: 3
Severity: ORANGE
Expected scope:
- `.github/workflows/stabilization.yml`

Do this in the same C-bridge repair PR or immediately after Repair 2.

### ASan/UBSan

Build `litt_json.c` with ASan/UBSan and build/link:

- `alt/src/native/litt_c.cpp`
- `alt/src/native/litt_c_tests.cpp`
- sanitized JSON object

Run it under the same `ASAN_OPTIONS` and `UBSAN_OPTIONS` used by the existing sanitizer job.

### Windows

Replace the compile-only C-bridge evidence with a build-and-run contract:

- compile/link `litt_c.cpp`
- compile/link `litt_c_tests.cpp`
- link the existing `litt_json.obj`
- include the same Windows system libraries required by the Engine-containing bridge object
- execute the resulting test binary

Keep the Linux packaged external C-consumer gate.

Do not add unsupported Windows package-archive requirements.

## Repair 4: repair the live littcore README examples

Priority: 4
Severity: ORANGE
Expected scope:
- `alt/src/native/littcore/README.md`

Update examples to current API names and signatures.

At minimum fix:

- `Quatf` -> `Quat`
- `Rayf` -> `Ray`
- `HitInfof` -> `HitInfo`
- `litt::math::ray_aabb` -> `litt::ray_aabb`
- `litt::math::ray_triangle` -> `litt::ray_triangle`
- `Mat4f::rotation_y` -> current `Mat4::rot_y` or equivalent current alias usage
- ECS `create_entity/add_component/get_component/has_component` examples -> current `World::create/add/get/has`

Also correct the Transform example to set `Transform::position` rather than a nonexistent `data` member.

Validate the final snippets by compiling them against the packaged C++ SDK. Do not merely spell-check the names.

## Final validation and merge order

Recommended order:

1. Repair 1 in a focused branch/PR.
2. Repair 2 and Repair 3 together because the new semantics need the strengthened C-bridge gates.
3. Repair 4 as a docs-only cleanup, preferably in the same final repair PR only if it stays reviewable.
4. Run all targeted tests.
5. Run full Stabilization on each repair PR.
6. Merge only green PRs.
7. Verify the exact merged `main` SHA receives a fully green Stabilization run.
8. Re-run this audit against that merged SHA.

## Things this guide intentionally does not repair

Do not spend this repair cycle on:

- GPU hardware certification;
- Vulkan/DX12/OpenGL/Metal implementation;
- historical ROADMAP wording;
- GitHub repository description;
- old non-release `litt_world.cpp`;
- C bridge persistence semantics not yet specified;
- C bridge rendering/quality controls outside the supported bridge boundary;
- asset async/options behavior outside the current synchronous release contract.

If new evidence proves one of those is part of the live supported contract, audit it separately before changing it.
