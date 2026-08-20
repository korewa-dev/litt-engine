# Ray Tracing (Path Tracer)

> Deep-dive into the path tracing implementation in `crates/pathtracer/`.

## Shader Pipeline

| Shader | File | Stage |
|--------|------|-------|
| Ray Gen | `shaders/pathtracer/raygen.rgen.glsl` | Ray generation |
| Closest Hit | `shaders/pathtracer/chit.rchit.glsl` | BRDF evaluation |
| Miss | `shaders/pathtracer/miss.rmiss.glsl` | Background color |

## Acceleration Structure Pipeline

```
BLAS Builder
  ├─ Add geometries (triangles)
  ├─ Query build sizes
  ├─ Allocate BLAS buffer (VMA)
  └─ Build acceleration structure

TLAS Builder
  ├─ Add instances (BLAS handles + transforms)
  ├─ Create instance buffer
  ├─ Query build sizes
  ├─ Allocate TLAS buffer (VMA)
  └─ Build acceleration structure
```

## BRDFs

| BRDF | Description |
|------|-------------|
| Lambertian Diffuse | Perfectly diffuse reflection |
| GGX Specular | Microfacet specular lobe |
| Metal | Metallic Fresnel |
| Dielectric | Non-metallic Fresnel |

## Russian Roulette Termination

Randomly terminates rays below a brightness threshold to control memory and compute.

## Temporal Accumulation

Progressive rendering accumulates samples across frames using motion vectors.

## ReSTIR (Planned)

Support for ReSTIR-style reservoir sampling is architecture-ready. See [../../ROADMAP.md](../../ROADMAP.md#phase-14).
