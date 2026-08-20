# Rendering Documentation

Render system, path tracer, and frame graph internals.

## Files

| File | Content |
|------|---------|
| [render-system.md](./render-system.md) | ECS-driven rendering pipeline |
| [path-tracer.md](./path-tracer.md) | Path tracer crate internals |
| [frame-graph.md](./frame-graph.md) | Pass ordering, shader compilation |

## Architecture

```
Renderer (litt-renderer)
    |
    v
Path Tracer (litt-pathtracer)
    |
    v
FidelityFX (litt-fidelityfx)
    |
    v
Display (Present)
```

See [../graphics-api/graphics-api-status.md](../graphics-api/graphics-api-status.md) for API status.
