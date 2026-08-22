# Litt Engine Architecture

## High-Level Overview
Application Layer -> Platform Layer -> Vulkan Backend (VMA + AMD AGS) -> Renderer -> Path Tracer -> FidelityFX -> Display

## Crate Dependencies
```
litt (root)
â”œâ”€â”€ litt-math           (no dependencies)
â”œâ”€â”€ litt-platform       (ash, bytemuck, platform-specific)
â”œâ”€â”€ litt-vulkan         (ash, ash-window, vma, bytemuck, litt-math, litt-platform, ags)
â”œâ”€â”€ litt-renderer       (ash, bytemuck, litt-math, litt-vulkan)
â”œâ”€â”€ litt-pathtracer     (ash, bytemuck, litt-math, litt-vulkan, litt-renderer, vma)
â””â”€â”€ litt-fidelityfx     (ash, bytemuck, litt-math, litt-vulkan, vma)
```

## Render Pipeline
```
Frame Start
  -> AMD AGS (GPU Detection & Optimization)
  -> Path Trace (Compute Shader)
  -> FidelityFX Ray Reconstruction (Denoiser)
  -> FidelityFX FSR 3.1.5
      â”œâ”€ Create Pass (temporal accumulation)
      â”œâ”€ Compensate Pass (motion vectors)
      â”œâ”€ Upscaler Pass (upscaling)
      â””â”€ Frame Gen Pass (frame generation)
  -> FidelityFX CAS (sharpening)
  -> Tonemap
  -> Present
```

## AMD AGS (Adaptive Graphics Selection)
```
GpuManager
  â”œâ”€ enumerate_adapters() - Find all GPUs
  â”œâ”€ add_gpu() - Add physical device
  â”œâ”€ select_best() - Score and select optimal GPU
  â””â”€ get_selected() - Get selected GPU properties

GpuProperties
  â”œâ”€ vendor: GpuVendor (AMD, Intel, Samsung, Moore Threads)
  â”œâ”€ rdna_gen: u32 (2, 3, or 4)
  â”œâ”€ npu_support: bool
  â”œâ”€ fsr4_support: bool
  â””â”€ npu_tops: f32

AgsHints
  â”œâ”€ wave32_enabled: bool (RDNA 2/3 optimization)
  â”œâ”€ sustained_encoding: bool (RDNA 3+ optimization)
  â”œâ”€ pipeline_cache: bool
  â””â”€ shader_core_hints: bool
```

---

## Memory Management (VMA)
```
VMA Allocator
  â”œâ”€ allocate_buffer() - GPU buffer allocation
  â”œâ”€ allocate_image()  - GPU image allocation
  â”œâ”€ map_memory()      - Host-visible mapping
  â”œâ”€ flush_allocation() - Cache flush
  â””â”€ free_*()          - Cleanup
```

## Data Flow
```
Scene Data (CPU)
  â†’ upload_scene() â†’ GPU Buffers (Triangles, Spheres, Lights, Materials)
  â†’ build_blas_from_triangles() â†’ BLAS (Bottom-Level AS)
  â†’ build_scene_acceleration() â†’ TLAS (Top-Level AS)
  â†’ Shader Binding Table (SBT)
  â†’ Ray Tracing Pipeline Execution
```

## Acceleration Structure Pipeline
```
BLAS Builder
  â”œâ”€ Add geometries (triangles)
  â”œâ”€ Query build sizes
  â”œâ”€ Allocate BLAS buffer (VMA)
  â””â”€ Build acceleration structure

TLAS Builder
  â”œâ”€ Add instances (BLAS handles + transforms)
  â”œâ”€ Create instance buffer
  â”œâ”€ Query build sizes
  â”œâ”€ Allocate TLAS buffer (VMA)
  â””â”€ Build acceleration structure
```

## Interactive Diagram
See [litt-engine-architecture.html](../litt-engine-architecture.html) for a full interactive visualization.

