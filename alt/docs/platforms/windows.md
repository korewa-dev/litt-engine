# Windows Platform Notes

Windows is part of the release verification matrix for the native runtime, public headers, networking, scene persistence, WorldGen, and the GPU/software contract.

## Graphics status

The current release-supported Windows graphics contract does **not** prefer DX12 or fall back to Vulkan. Generic DX12 and Vulkan device paths are unavailable today.

The Windows GPU contract is green because it verifies truthful behavior:

- software/headless rendering remains usable within its tested scope;
- requesting generic DX12 or Vulkan must not fabricate success;
- an unavailable backend is rejected or its initialization returns failure;
- named GPU vendors are not physically certified by CI.

See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md).

## DX12 and Vulkan promotion

Future DX12 or Vulkan promotion requires a real device path, adapter enumeration, queues, resource allocation, swapchain, shaders/pipelines, triangle and OBJ/material rendering, resize handling, sustained frames, shutdown/error paths, validation tooling, and physical hardware evidence.

No AMD, Intel Arc, NVIDIA, Moore Threads, or other Windows GPU should be described as Litt-supported for DX12/Vulkan until that evidence exists.

## Driver notes

Vendor driver documentation can be useful when performing future physical certification, but driver API support alone is not Litt Engine support. Record the exact GPU, driver, OS, backend, validation output, and tested workload for each certification run.

## Current Windows release gate

Use the `windows-native` job in `.github/workflows/stabilization.yml` as the authoritative Windows verification contract.
