# Litt Engine Release Readiness Roadmap

Goal: preserve the green supported runtime while increasing release proof before increasing supported surface area.

## Current baseline

The supported Windows/Linux runtime baseline is green on the exact post-maintenance main SHA after the full Stabilization matrix, including repository hygiene and cross-OS WorldGen.

Completed release gates:
- GCC and Clang compiler matrix;
- Windows/MSVC native contract;
- ASan/UBSan stabilization;
- native C runtime and C bridge/SDK consumer;
- minimal TCP contract;
- software/headless GPU contract;
- generated-game and WorldGen cross-OS contract;
- repository hygiene with no tracked compiler intermediates.

## Current graphics gate

Software/headless rendering is green within its tested scope.

Generic Vulkan and DX12 are **GREEN only for their fail-fast contract**: the engine reports them unavailable rather than fabricating success. No named GPU vendor or model is physically certified for Litt Vulkan/DX12 rendering yet.

Graphics promotion requires real backend implementation plus physical evidence. See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md).

## Active release engineering work

| Area | State |
|---|---|
| Public capability truthfulness | ACTIVE |
| Reproducible release package/checksums | TODO |
| Required-check repository protection | TODO |
| Parser/network fuzz smoke | TODO |
| Allocation/overflow fault injection | TODO |
| Persistence property tests | TODO |
| Lifecycle soak | TODO |
| Concurrency proof | TODO |
| macOS support decision | TODO |
| Physical GPU certification | TODO |
| Litt GUI integration | DEFERRED: ENGINE FIRST |

## Promotion rule

A partial or experimental subsystem becomes release-supported only after it has an explicit behavior/error contract, regression tests, applicable sanitizer/platform coverage, external-consumer proof where relevant, truthful documentation, and post-merge main validation.

## GPU certification rule

Software CI cannot substitute for real GPU evidence. For each promoted DX12/Vulkan configuration record the GPU model, driver, OS, backend initialization, adapter/device/queue/swapchain creation, shader/pipeline creation, buffers/textures, triangle, OBJ/material rendering, resize, 1000+ frames, shutdown, and validation/debug errors.

## Merge policy

Use the lifecycle:

`UNDERSTAND -> SPECIFY -> TEST -> IMPLEMENT -> VALIDATE -> AUDIT -> MERGE -> VALIDATE MAIN`

Branch-only success is not release proof. A red supported-path gate must not be merged except through an explicit administrative emergency process.
