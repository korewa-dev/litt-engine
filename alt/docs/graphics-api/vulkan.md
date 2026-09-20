# Vulkan Backend

**Status: unavailable in the generic Litt Engine renderer.**

The repository contains Vulkan-oriented experiments and placeholder code, but the current `VulkanDevice::initialize()` path deliberately returns failure. There is no release-supported Vulkan instance/device/swapchain/render pipeline in the generic backend.

Do not infer support from filenames, build flags, backend enums, vendor IDs, or historical roadmap documents.

## Promotion gate

Vulkan remains unavailable until the complete promotion sequence in [graphics-api-status.md](graphics-api-status.md) is implemented, validation layers and failure paths are exercised, sustained rendering and shutdown are tested, and physical hardware evidence exists for any vendor-specific claim.

Until then, callers must handle Vulkan selection as unavailable.
