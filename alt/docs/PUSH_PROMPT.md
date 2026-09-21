# Litt Engine Documentation Push Prompt

> **Historical artifact.** This prompt describes an older architecture/documentation phase. It is not a release-support statement and must not be used to claim working Vulkan, DX12, FidelityFX, NPU, vendor-GPU, Rust-crate, or cross-platform backend support. Use `SUPPORTED_RUNTIME.md` and `graphics-api/graphics-api-status.md` for current status.

## Current documentation rule

When reusing any material from this prompt:

- preserve useful historical/design context;
- label Vulkan and DX12 as experimental/unavailable unless the graphics status ledger is promoted;
- never turn upstream driver/API compatibility into Litt Engine hardware certification;
- never describe AMD, Intel Arc, NVIDIA, Moore Threads, ARM, mobile, Apple, Steam Deck, or RISC-V GPUs as supported without physical evidence;
- treat removed Rust-era paths as historical references only;
- prefer the native runtime and current repository layout.

## Historical context

Earlier documentation work described a target engine architecture with ECS, Vulkan/DX12, path tracing, FidelityFX, NPU acceleration, and multiple hardware targets. Those items represented intended architecture or experiments, not a verified release matrix.

Any generated documentation must now include an explicit support boundary and link to the authoritative release contract.
