# Litt Engine Architecture

## High-Level Overview
Application Layer -> Renderer Layer -> FidelityFX Layer -> Path Tracer Layer -> Vulkan Layer -> Platform Layer -> Math Library

## Crate Dependencies
litt (root)
├── litt-math           (no dependencies)
├── litt-platform       (ash, bytemuck, platform-specific)
├── litt-vulkan         (ash, bytemuck, litt-math, litt-platform)
├── litt-renderer       (ash, bytemuck, litt-math, litt-vulkan)
├── litt-pathtracer     (ash, bytemuck, litt-math, litt-vulkan, litt-renderer)
└── litt-fidelityfx     (ash, bytemuck, litt-math, litt-vulkan)

## Render Pipeline
Frame Start -> Path Trace (Compute) -> FidelityFX Ray Reconstruction -> FidelityFX FSR 2 -> FidelityFX CAS -> Tonemap -> Present

## Data Flow
Scene Data (CPU) -> Upload to GPU buffers -> Create acceleration structures -> Shader binding (SBT)
