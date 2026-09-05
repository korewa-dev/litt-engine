# Litt Engine Architecture

## High-Level Overview
Application Layer -> Platform Layer -> Vulkan Backend (VMA + AMD AGS) -> Renderer -> Path Tracer -> FidelityFX -> Display

## Crate Dependencies
```
litt (root)
 litt_math           (no dependencies)
 litt_platform       (ash, bytemuck, platform-specific)
 litt_vulkan         (ash, ash-window, vma, bytemuck, litt_math, litt_platform, ags)
 litt_renderer       (ash, bytemuck, litt_math, litt_vulkan)
 litt_pathtracer     (ash, bytemuck, litt_math, litt_vulkan, litt_renderer, vma)
 litt_fidelityfx     (ash, bytemuck, litt_math, litt_vulkan, vma)
```

## Render Pipeline
```
Frame Start
  -> AMD AGS (GPU Detection & Optimization)
  -> Path Trace (Compute Shader)
  -> FidelityFX Ray Reconstruction (Denoiser)
  -> FidelityFX FSR 3.1.5
       Create Pass (temporal accumulation)
       Compensate Pass (motion vectors)
       Upscaler Pass (upscaling)
       Frame Gen Pass (frame generation)
  -> FidelityFX CAS (sharpening)
  -> Tonemap
  -> Present
```

## AMD AGS (Adaptive Graphics Selection)
```
GpuManager
   enumerate_adapters() - Find all GPUs
   add_gpu() - Add physical device
   select_best() - Score and select optimal GPU
   get_selected() - Get selected GPU properties

GpuProperties
   vendor: GpuVendor (AMD, Intel, Samsung, Moore Threads)
   rdna_gen: u32 (2, 3, or 4)
   npu_support: bool
   fsr4_support: bool
   npu_tops: f32

AgsHints
   wave32_enabled: bool (RDNA 2/3 optimization)
   sustained_encoding: bool (RDNA 3+ optimization)
   pipeline_cache: bool
   shader_core_hints: bool
```

---

## Memory Management (VMA)
```
VMA Allocator
   allocate_buffer() - GPU buffer allocation
   allocate_image()  - GPU image allocation
   map_memory()      - Host-visible mapping
   flush_allocation() - Cache flush
   free_*()          - Cleanup
```

## Data Flow
```
Scene Data (CPU)
   upload_scene()  GPU Buffers (Triangles, Spheres, Lights, Materials)
   build_blas_from_triangles()  BLAS (Bottom-Level AS)
   build_scene_acceleration()  TLAS (Top-Level AS)
   Shader Binding Table (SBT)
   Ray Tracing Pipeline Execution
```

## Acceleration Structure Pipeline
```
BLAS Builder
   Add geometries (triangles)
   Query build sizes
   Allocate BLAS buffer (VMA)
   Build acceleration structure

TLAS Builder
   Add instances (BLAS handles + transforms)
   Create instance buffer
   Query build sizes
   Allocate TLAS buffer (VMA)
   Build acceleration structure
```

## Interactive Diagram
See [litt_engine-architecture.html](../litt_engine-architecture.html) for a full interactive visualization.



