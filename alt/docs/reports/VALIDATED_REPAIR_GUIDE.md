# Validated Repair Guide

Source audit: `alt/docs/reports/VALIDATED_HEAVY_AUDIT.md`
Audit baseline: `657455360e4f0208e7fe286a46d41f1bffc0b863`

This guide contains only defects/inconsistencies independently confirmed against current code, the authoritative support contract, current tests, exact-main CI evidence, documentation intent, and historical/archive status.

## Non-negotiable rules

Use a fresh branch from current `main`.

Do not weaken tests, CI gates, support contracts, failure behavior, or intentional project constraints to obtain a green result.

Do not merge or cherry-pick `audit-c-bridge-components-20260922`: its narrow-looking bridge change is actually +26/-305 lines in `litt_c.cpp`. Do not use stale `c-bridge-component-contract-20260920` as a repair base.

For each repair use:

`understand -> specify -> test -> implement -> validate -> audit -> merge -> validate exact main`

GPU physical certification may remain on hold.

## Repair 1: correct inside-origin ray/AABB distance

Severity: RED
Priority: 1

Affected paths:

- `alt/src/native/littcore/litt_math.h`
- a currently gated canonical C++ contract test, preferably `alt/src/native/littcore/litt_engine_tests.cpp`

### Evidence proving the defect

Current `ray_aabb` computes slab entry/exit distances but uses:

```cpp
float t = tmin > r.t_min ? tmin : r.t_max;
```

For an origin inside an AABB, this chooses the ray's global far bound instead of the computed slab exit. Example: box `[-1,1]^3`, origin `(0,0,0)`, direction `+X`, interval `[0,10]` yields computed exit `1` but current code returns `10`, placing the reported hit outside the box.

The existing ray/AABB regression starts outside the box, so it cannot exercise this branch. The authoritative runtime contract promotes C++17 math and the packaged C++ SDK, so this is live supported behavior.

### Appropriate change

Test first, then replace only the erroneous fallback selection so an inside-origin ray uses computed `tmax`. Preserve unrelated boundary and zero-direction policy.

Add gated cases for:

1. outside-origin entry;
2. inside-origin exit;
3. inside-origin exit beyond ray `t_max` -> no hit;
4. parallel miss outside a slab.

### Validation checks

At minimum:

```sh
make engine-contract-test
make release-hardening-test
make -C alt/src/native cpp-sdk-test
```

Then require the full Stabilization workflow on the repair PR and exact merged main.

Regression risk: low if the change is limited to the fallback selection; medium if slab/boundary logic is refactored. Do not refactor it in this repair.

Merge readiness: not ready until the new inside-origin regression fails before the fix, passes after it, existing math/SDK contracts remain green, and full Stabilization passes.

## Repair 2: make C bridge component configuration fail-closed and truthful

Severity: ORANGE
Priority: 2

Affected paths:

- `alt/src/native/litt_c.h`
- `alt/src/native/litt_c.cpp`
- `alt/src/native/litt_c_tests.cpp`

### Evidence proving the inconsistency

Current `litt_world_add_component` discards `config_json` with `(void)config_json`, sets only a presence bit, and returns success. No per-type configuration is stored. Existing tests pass `"{}"` and only prove the presence bit.

The authoritative support contract promotes the versioned C bridge for entity/component/transform use. Issue #53 forbids successful no-ops and requires unavailable operations to fail explicitly. Therefore meaningful configuration cannot truthfully return success while being ignored.

### Appropriate change

Do not invent Mesh/Physics/Light/etc. schemas. Preserve the implemented presence-only behavior and make unsupported configuration explicit:

- accept `nullptr`, empty string, and strict empty object as default/no configuration;
- reject meaningful objects, malformed JSON, arrays, scalars, and other unsupported configuration before changing the bitmap;
- keep Transform configuration on its existing setter/getter API;
- document the presence-only limitation in `litt_c.h`.

Use the existing strict JSON parser already in the native dependency graph if possible. Keep the implementation diff local and small.

### Test first

Add cases proving:

- Mesh + `nullptr` succeeds;
- Mesh + `""` succeeds;
- Mesh + `"{}"` succeeds;
- meaningful object fails and leaves presence false;
- malformed JSON fails and leaves presence false;
- array/scalar fail and leave presence false;
- invalid entity/type behavior remains unchanged;
- Transform setter/getter behavior remains unchanged.

Confirm a meaningful-config test fails before implementation.

### Validation checks

Run the bridge contract and inspect the diff for accidental deletion or unrelated changes. Then use the strengthened CI in Repair 3 and require full Stabilization.

Regression risk: medium because this intentionally changes previously accepted-but-ignored inputs into explicit failure. That is appropriate because successful ignored configuration violates the promoted truthfulness constraint. Do not add typed schemas in the same patch.

Merge readiness: ready only when presence-only defaults still work, unsupported configuration fails without mutation, bridge ABI remains compatible, Windows/Linux/sanitizer gates are green, and exact merged main is green.

## Repair 3: close C bridge Windows and sanitizer proof gaps

Severity: ORANGE
Priority: 3

Affected path:

- `.github/workflows/stabilization.yml`

### Evidence proving the proof gap

Exact-main Stabilization run `35785302169` shows:

- Linux bridge tests execute;
- Windows only compiles the C bridge and does not execute `litt_c_tests.cpp`;
- the ASan/UBSan job does not build/run the bridge contract.

`SUPPORTED_RUNTIME.md` promotes the C ABI/SDK and requires Windows/Linux coverage where applicable plus sanitizer coverage for memory-unsafe promoted paths. The existing gate therefore falls short of the repository's own promotion rule.

### Appropriate change

In Windows CI, build/link and execute `litt_c_tests.cpp` with `litt_c.cpp`, the existing JSON object, and the system libraries already required by the bridge object.

In Linux sanitizer CI, build the bridge dependencies and execute the bridge contract under the existing ASan/UBSan options.

Keep the existing Linux external packaged C consumer gate. Do not add a Windows package archive requirement because the support contract explicitly does not claim one.

### Validation checks

Inspect workflow syntax, then require the new Windows bridge execution and sanitizer bridge execution to pass in CI along with every existing Stabilization job.

Regression risk: low to medium. This changes proof, not runtime behavior, but may expose latent defects. If it does, fix those defects; do not weaken the new gates.

Merge readiness: ready only when the new jobs/steps actually execute tests rather than compile-only checks and full Stabilization is green.

## Repair 4: repair active littcore README examples against the current API

Severity: ORANGE
Priority: 4

Affected path:

- `alt/src/native/littcore/README.md`

### Evidence proving the inconsistency

The active quick-start README uses invalid current API names/signatures including `Quatf`, `Rayf`, `HitInfof`, `litt::math::ray_aabb`, `litt::math::ray_triangle`, `Mat4f::rotation_y`, ECS `create_entity/add_component/get_component/has_component`, and `Transform::data`.

Current code exposes `Quat`, `Ray`, `HitInfo`, namespace-level `ray_aabb/ray_triangle`, `Mat4::rot_y`, ECS `create/add/get/has`, and the current Transform member contract.

Revalidation also proved that `Vec2f`, `Vec3f`, `Vec4f`, `Mat4f`, and `Aabbf` are intentional aliases in `litt_math.h`. They are valid and must not be changed merely for naming consistency.

### Appropriate change

Update only invalid names, calls, and member usage. Keep valid aliases if desired. Do not rewrite the architecture or claim unavailable backends.

### Validation checks

Extract or mirror the final quick-start, math, and ECS examples into compile checks against the packaged C++ SDK. A documentation-only spelling review is insufficient.

Regression risk: low if limited to current public API examples.

Merge readiness: ready only when every presented snippet compiles against the packaged SDK and the resulting documentation matches `SUPPORTED_RUNTIME.md`.

## Intentionally excluded from repair

Do not spend this repair cycle changing GPU certification status, implementing Vulkan/DX12/OpenGL/Metal, editing historical/research backend documents solely for old claims, changing the GitHub repository description, promoting old `litt_world.cpp`, inventing C bridge persistence semantics, expanding bridge renderer controls outside the supported boundary, or implementing asset async/options behavior outside the synchronous release contract.

`alt/src/native/littcore/SUMMARY.md` also contains old-style examples, but its current documentation intent is ambiguous and it reads as a historical creation summary. It is deliberately excluded until maintainers establish it as an active user-facing contract.

## Recommended merge order

1. Repair 1 as a focused correctness PR.
2. Repairs 2 and 3 together or in immediately adjacent PRs so the changed bridge semantics are protected by the stronger gates.
3. Repair 4 as a focused documentation/API-proof PR.
4. Run targeted tests for each repair.
5. Require full Stabilization on every PR.
6. Merge only green, scoped diffs.
7. Require full Stabilization on the exact merged main SHA.
8. Re-run the heavy audit against that SHA.

Current main remains CI-green but not final heavy-audit merge-ready while RED-1 is present. GPU physical certification may remain on hold.