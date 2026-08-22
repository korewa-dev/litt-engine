# Vulkan Backend

> Full Vulkan 1.3 backend in `crates/vulkan/`.

## Modules

| Module | File | Purpose |
|--------|------|---------|
| instance | `instance.rs` | Vulkan instance, layer/query validation |
| device | `device.rs` | Physical device selection, logical device creation |
| swapchain | `swapchain.rs` | Swapchain creation, image acquisition, presentation |
| allocator | `allocator.rs` | VMA memory allocator integration |
| pipeline | `pipeline.rs` | Graphics and compute pipeline creation |
| ray_tracing | `ray_tracing.rs` | BLAS/TLAS build, ray tracing pipeline |

## BLAS/TLAS Pipeline

```
Scene Data (CPU)
  -> upload_scene() -> GPU Buffers
  -> build_blas_from_triangles() -> BLAS
  -> build_scene_acceleration() -> TLAS
  -> Shader Binding Table (SBT)
  -> Ray Tracing Pipeline Execution
```

## Memory Management (VMA)

```
VMA Allocator
  ├─ allocate_buffer()
  ├─ allocate_image()
  ├─ map_memory()
  ├─ flush_allocation()
  └─ free_*()
```

See [../platforms/amd-rdna.md](../platforms/amd-rdna.md) for RDNA-specific Vulkan tuning.
