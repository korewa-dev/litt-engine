# Path Tracer

> Deep dive into the `litt_pathtracer` module internals.

## Crate: litt_pathtracer

**Path:** `native/littcore/pathtracer/src/`
**Dependencies:** ash, bytemuck, litt_math, litt_vulkan, litt_renderer, vma

### Modules

| Module | File | Purpose |
|--------|------|---------|
| scene | `scene.rs` | Scene data (triangles, spheres, lights, materials) |
| tracer | `tracer.rs` | GPU path tracer dispatch, temporal accumulation |
| material | `material.rs` | BRDF types (Lambertian, GGX, metal, dielectric) |
| rng | `rng.rs` | PCG random number generator for Monte Carlo |

## GPU Buffers

```cpp
pub struct PathTracerBuffers {
    pub triangles: Buffer,
    pub spheres: Buffer,
    pub lights: Buffer,
    pub materials: Buffer,
    pub sbt: Buffer,
}
```

## Shader Files

| Shader | File | Stage |
|--------|------|-------|
| Ray Gen | `shaders/pathtracer/raygen.rgen.glsl` | Ray generation |
| Closest Hit | `shaders/pathtracer/chit.rchit.glsl` | BRDF evaluation |
| Miss | `shaders/pathtracer/miss.rmiss.glsl` | Background color |
| Tonemap | `shaders/compute/tonemap.comp.glsl` | HDR to LDR |
| TAA | `shaders/compute/taa.comp.glsl` | Temporal accumulation |
| Dither3D | `shaders/dither3d/mesh.frag.glsl` | Surface-stable fractal dithering |

## Render Pipeline

```
Frame Start
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

## BRDFs

| BRDF | Description | Shader |
|------|-------------|--------|
| Lambertian Diffuse | Perfectly diffuse | `chit.rchit.glsl` |
| GGX Specular | Microfacet specular | `chit.rchit.glsl` |
| Metal | Metallic fresnel | `material.rs` |
| Dielectric | Non-metallic fresnel | `material.rs` |

## Russian Roulette

Randomly terminates rays below a brightness threshold.

## Temporal Accumulation

Progressive rendering accumulates samples across frames using motion vectors.

## Roadmap

### Short-term
- [ ] Optimize BLAS rebuild for dynamic scenes
- [ ] Add instance buffer invalidation tracking

### Hardware-Specific
- **RDNA / AMD:** Wave64 for ray tracing, async compute for BLAS rebuild
- **Moore Threads:** MUSA ray tracing acceleration
- **ARM / Mobile:** Reduced bounce count, spatial upscaling
- **RISC-V:** Software ray-triangle intersection fallback

