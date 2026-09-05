# Procedural Asset Math Cookbook

Every algorithm an AI agent needs to generate models, environments and props
from pure math. Language-agnostic pseudocode; a runnable Python reference
implementation lives at `template/tools/procedural_assets.py`.

Conventions everywhere: **meters, Y-up, right-handed, CCW front faces,
origin at base center.**

---

## 0. Core Rules

### Face winding -> normal direction
Triangle (A, B, C) has normal `n = normalize((B-A) x (C-A))`.
If n points away from where a viewer stands, reverse to (A, C, B).
All tables below are pre-verified for correct outward winding.

### Flat shading
One normal per triangle, reused by its 3 vertices (`f a//n b//n c//n`).
Cheap, crisp low-poly look, no smoothing groups needed.

---

## 1. Primitive: Box

Half-extents (hx, hy, hz) centered at (cx, cy, cz). Corner table (indices 0-7):

```
0:(+x,-y,-z)  1:(+x,-y,+z)  2:(+x,+y,+z)  3:(+x,+y,-z)
4:(-x,-y,-z)  5:(-x,-y,+z)  6:(-x,+y,+z)  7:(-x,+y,-z)
```

Quad faces, each emitted as triangles (a,b,c)+(a,c,d):

| Face | Corners | Outward normal |
|------|---------|----------------|
| +X | 0,3,2,1 | (1,0,0) |
| -X | 4,5,6,7 | (-1,0,0) |
| +Y | 2,3,7,6 | (0,1,0) |
| -Y | 0,1,5,4 | (0,-1,0) |
| +Z | 1,2,6,5 | (0,0,1) |
| -Z | 0,4,7,3 | (0,0,-1) |

Cost: 8 unique verts, 12 tris. Composes: buildings, crates, furniture, chimneys.

---

## 2. Primitive: Gable Roof Prism

Base rectangle center (cx, base_y, cz), half-width rx, half-depth rz,
ridge height rh. Six points:

```
L0=(-rx, 0, -rz)  L1=(-rx, 0, +rz)   ridge: TB=(0, rh, -rz)  TF=(0, rh, +rz)
R0=(+rx, 0, -rz)  R1=(+rx, 0, +rz)
```

Quads/tris (winding verified): left slope (L0,L1,TF,TB), right slope
(R1,R0,TB,TF), gable +Z (R1,TF,L1), gable -Z (L0,TB,R0). No bottom face.
Add overhang by inflating rx/rz past the wall footprint (~0.25 m looks right).

---

## 3. Primitive: Pyramid

Base center (cx, base_y, cz), half-extents hx/hz, height h, apex A=(cx, base_y+h, cz).
Four side triangles, base corners BL/BR/TL/TR:

| Side | Order |
|------|-------|
| +Z | BR, A, BL |
| -Z | BL, A, BR |
| +X | BR(back), A, FR |  *(front-right = +z corner)* |
| -X | FL, A, BL(back-left) |

Rule of thumb that generates these: for each edge of the base loop (walked
counter-clockwise seen from above), emit tri(edge_start, A, edge_end).
No bottom face. Composes: tree canopies, spires, roofs, tents.

---

## 4. Primitive: Cylinder / Cone (n-gon)

N segments (8-12 reads low-poly-clean). Ring points at angle step
theta = 2*pi*i/N, radius r, heights y0/y1:

```
P(i, y, rad) = (cx + rad*cos(theta_i), y, cz + rad*sin(theta_i))
```

Side quads between bottom ring (r0) and top ring (r1), for i -> i+1:
quad( P(i,y0,r0), P(i,y1,r1), P(i+1,y1,r1), P(i+1,y0,r0) ).

Caps: top fan tri(centerTop, P(i,y1,r1), P(i+1,y1,r1)); bottom fan reversed.
Set r1=0 and replace top ring with a single apex point -> cone.
Composes: trunks, columns, barrels, poles.

---

## 5. Environment: Terrain from Noise

Heightfield -> grid mesh. Resolution R x R cells over world size S:

1. Height per lattice point: `h(x,z) = fbm(x*f, z*f, seed)` in [0,1],
   scaled by max_height. fbm = fractal Brownian motion:

```
fbm(x,y) = SUM(o=0..octaves-1) amp_o * value_noise(x*freq_o, y*freq_o)
amp_{o+1} = amp_o * 0.5 ; freq_{o+1} = freq_o * 2 ; normalize by SUM(amp)
```

2. value_noise: bilinear-interpolate hashed random values at the 4 surrounding
   integer lattice points, with smoothstep easing t*t*(3-2t) on the fractions.
   Hash (integer mix, deterministic, no libraries):
   `n = (ix*73856093) XOR (iy*19349663) XOR (seed*126271)`, map u32 -> [0,1].

3. Triangulate each cell (corners p00,p10,p11,p01) as
   (p00,p01,p11) + (p00,p11,p10) - verified +Y normals on flat ground.

Deterministic: same seed -> byte-identical world. Always expose the seed.

---

## 6. Environment: Scatter Placement (trees, rocks, props)

Dart throwing with minimum spacing, fully deterministic:

```
rng = seeded_xorshift(seed)
points = []
for attempt in 1..max_attempts:
    p = (rng.uniform(0,S), rng.uniform(0,S))
    if fbm_density(p) > threshold and distance(p, all points) > min_dist:
        points.push(p)
```

Placement rules that read as intentional level design:
- buildings snap to grid cells / roads; doors face the road
- vegetation only where density-noise passes threshold (forest patches form)
- jitter positions +-0.3 m off perfect grids so it feels hand-placed
- never intersect: check horizontal distance > sum of bounding radii

---

## 7. Deterministic RNG (xorshift32, no dependencies)

```
state = seed or 0x9E3779B9
next(): x ^= x<<13; x ^= x>>17; x ^= x<<5  (mask u32 each op)
uniform(lo,hi) = lo + (hi-lo) * next()/2^32
```

Never use wall-clock randomness for assets: another agent must be able to
regenerate your exact file.

---

## 8. Procedural Textures (optional, advanced)

A PNG is zlib-compressed scanlines behind an 8-byte signature - writable with
python stdlib (struct + zlib, ~30 lines). Generate pixels from the same
value_noise: stone = fbm quantized to 4 grey bands; grass = green channel +
high-frequency noise speckle. If a style beyond flat-color/noise is needed,
use the image-generation route in ai_asset_creation.md Route 2 instead of
fighting math.

---

## 9. Collision Approximation

Physics needs simple volumes matching art bounds:
- box mesh -> box collider of its half-extents
- cylinder/tree -> capsule (radius = max ring r, height spans y-range)
- terrain -> heightfield collider sampled from the same fbm function

Generate collider parameters IN the same script run so art and physics
can never drift apart.
