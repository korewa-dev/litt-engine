# Litt Engine Development Roadmap

## Phase 1: Foundation (Current)
- [x] Project structure
- [x] Custom math types
- [x] Platform abstraction layer
- [x] Vulkan backend skeleton
- [x] Path tracing shader stubs
- [x] FidelityFX integration points

## Phase 2: Core Rendering
- [x] Complete Vulkan device initialization
- [x] Implement swapchain management
- [x] Add command buffer recording
- [x] Implement descriptor set management
- [x] **Add VMA memory allocator integration** (vma crate)

## Phase 3: Path Tracing
- [x] **Implement BLAS/TLAS build pipeline**
- [x] Complete ray tracing shaders
- [x] Add Russian roulette termination
- [x] Implement temporal accumulation
- [ ] Add ReSTIR for light sampling

## Phase 4: FidelityFX
- [x] **Integrate FSR 3.1.5 with full compute shaders**
- [x] Add CAS sharpening
- [x] Implement Ray Reconstruction
- [x] **Add FSR 3 frame generation (create, compensate, upscaler, framegen)**

## Phase 5: Polish
- [ ] Binary size verification (< 1 MB)
- [ ] AMD RGP profiling integration
- [ ] Memory leak detection
- [ ] Error handling improvements
- [ ] Documentation completion

## Estimated Binary Sizes
| Phase | Windows | Linux | Android |
|-------|---------|-------|---------|
| Phase 1 | ~500 KB | ~400 KB | ~300 KB |
| Phase 2 | ~700 KB | ~600 KB | ~500 KB |
| Phase 3 | ~850 KB | ~750 KB | ~650 KB |
| Phase 4 | ~950 KB | ~850 KB | ~750 KB |
| Phase 5 | < 1 MB | < 900 KB | < 800 KB |
