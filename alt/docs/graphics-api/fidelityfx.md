# FidelityFX Research Notes

**Release status: experimental / not promoted.**

This document tracks FidelityFX, FSR, denoising, frame-generation, and related rendering research. Litt Engine does not currently have a release-supported FidelityFX path because its generic DX12 and Vulkan hardware backends are unavailable.

Upstream SDK or GPU compatibility must not be interpreted as Litt Engine certification.

## Current Litt contract

- no release-supported FidelityFX SDK integration is claimed;
- no FSR version is certified on a physical GPU by Litt;
- no vendor GPU matrix in this repository is a support matrix;
- generic DX12/Vulkan requests follow the fail-fast contract in [graphics-api-status.md](./graphics-api-status.md);
- physical support can only be promoted after the backend and hardware certification gates are complete.

## Design targets

The repository may contain shader experiments or design notes for:

- spatial and temporal upscaling;
- frame generation;
- contrast-adaptive sharpening;
- diffuse/specular denoising;
- ray reconstruction;
- vendor-neutral fallback strategies.

These are implementation or research surfaces, not release guarantees.

## Hardware evidence

When this subsystem is promoted, record at minimum the GPU/model, driver, OS, API backend, SDK version, input/output resolution, pipeline creation, resource lifecycle, sustained-frame run, shutdown behavior, validation/debug messages, and visual correctness checks.

For current release support, use [../SUPPORTED_RUNTIME.md](../SUPPORTED_RUNTIME.md).
