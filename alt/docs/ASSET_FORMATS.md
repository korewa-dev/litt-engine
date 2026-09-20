# Asset and material support

This document describes the supported subset, not aspirational format coverage.

## C++ AssetManager

### OBJ

Supported:
- `v`, `vt`, and `vn` records are parsed into their raw arrays.
- Faces use position indices. Positive and negative relative position indices are accepted.
- Polygons with 3 to 256 corners are triangulated as a fan.
- Unknown records are ignored.

Limits:
- Input file: 256 MiB.
- Positions, texture coordinates, normals: 4,194,304 entries each.
- Output indices: 12,582,912.

A malformed face, out-of-range position index, overlong logical line, unsupported extension, unreadable file, or exceeded limit fails the load. Failed loads are not cached.

The C++ model does not yet build a fully de-duplicated `v/vt/vn` vertex stream. Consumers requiring per-corner normals or UV seams must not treat that capability as implemented.

### TGA

Supported:
- Type 2 uncompressed true-color images.
- 24-bit BGR and 32-bit BGRA.
- Top-origin and bottom-origin images. Output is normalized to top-origin RGB/RGBA.

Rejected:
- Color-mapped images.
- RLE/compressed images.
- Grayscale images.
- Right-to-left origin.
- Zero dimensions or dimensions above 16384.
- Truncated payloads.

Pixel payloads are bounded by the 256 MiB asset limit. Failed loads are not cached.

### Shader assets

`AssetManager::loadShader` returns `nullptr`. No shader compiler/validator backend is connected to this API yet, so shader compilation is intentionally reported as unavailable.

## Native C OBJ loader

The native `lv_obj_load` path supports positions, texture coordinates, groups/objects, material switches, fan triangulation up to 64 corners, relative indices, and a small MTL subset (`Kd`, `Ke`, `map_Kd` parsing).

Malformed or out-of-range face indices fail the whole load. File size, parser vectors, index output, and remap storage are bounded.

## Materials

Raw asset material values have deterministic defaults:
- albedo: 0.8, 0.8, 0.8
- roughness: 0.5
- metalness: 0.0
- occlusion: 1.0
- emissive: 0.0

Missing MTL files/material names keep geometry usable and leave the native mesh material flags unset. This is the documented fallback behavior.
