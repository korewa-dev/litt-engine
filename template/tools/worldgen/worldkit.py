#!/usr/bin/env python3
"""WorldKit - shared engine for Litt genre world generators.

Every gen_*.py script next to this file imports WorldKit and produces a
complete playable-style world folder: meshes + materials + asset index +
scene file + world state + log entry. Deterministic: same seeds, same bytes.

Conventions: meters, Y-up, right-handed, CCW winding, origin at base center.
Algorithms reference: template/docs/procedural_asset_math.md

TRANSFORM CONVENTION (double-transform guard):
Prop meshes are modeled AT ORIGIN - footprint centered on x=z=0, base resting
at y=0 - and are positioned ONLY via their scene node's position/yaw. NEVER
bake world coordinates into vertices AND set node.position to the same
numbers: consumers that apply node transforms render such props displaced
~2x along every axis. Guards: assert_origin_centered(mb, tol) raises
TransformError on violation; recenter_mesh(mb) repairs baked offsets;
save_prop(..., enforce_origin=True) hard-fails off-center meshes at write
time, save_prop(..., auto_recenter=True) silently re-centers them. Sole
sanctioned exception: terrain chunks from emit_chunk() bake WORLD-space
vertices BY DESIGN (seamless chunking) and must sit at node.position
[0, *, 0] - never offset both.

PLACEMENT REGISTRY: worldkit.Placement tracks occupied x/z footprints,
rejects overlapping placements, and snaps props onto walkable surfaces
(ground_y / reserve_spot). Pass placement=reg to write_scene() to
collision-validate whole scenes before they hit disk. Every iteration order
is insertion order (plain dicts, no sets) - output bytes never depend on
hash randomization. Surfaces split into GROUND (walkable=True - provides
bbox-top Y, never blocks) and SOLID (walkable=False - occupies space,
rejects overlaps); pass blocks=True for standable platforms that do both.

Custom game with no preset generator? Copy gen_custom.py and edit the TODOs.
"""
import datetime
import hashlib
import json
import math
from pathlib import Path

NL = chr(10)
MODEL_BUDGET_KB = 500
ORIGIN_TOL = 0.05  # meters; default centroid tolerance for origin assertions

# ------------------------------------------------------------------ random
class Rng:
    """xorshift32 - cookbook section 7. Same seed, same world, forever."""
    def __init__(self, seed=1):
        self.s = seed & 0xFFFFFFFF or 0x9E3779B9

    def next_u32(self):
        x = self.s
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.s = x
        return x

    def uniform(self, lo=0.0, hi=1.0):
        return lo + (hi - lo) * (self.next_u32() / 4294967296.0)

    def pick(self, seq):
        return seq[self.next_u32() % len(seq)]

# ------------------------------------------------------------------- noise
def _lattice_hash(ix, iy, seed):
    n = (ix * 73856093) ^ (iy * 19349663) ^ (seed * 126271)
    return (n & 0xFFFFFFFF) / 0xFFFFFFFF

def value_noise(x, y, seed):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    u, v = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = _lattice_hash(ix, iy, seed)
    b = _lattice_hash(ix + 1, iy, seed)
    c = _lattice_hash(ix, iy + 1, seed)
    d = _lattice_hash(ix + 1, iy + 1, seed)
    return a*(1-u)*(1-v) + b*u*(1-v) + c*(1-u)*v + d*u*v

def fbm(x, y, seed, octaves=4, persistence=0.5, lacunarity=2.0):
    amp, freq, total, norm = 1.0, 1.0, 0.0, 0.0
    for _ in range(octaves):
        total += amp * value_noise(x * freq, y * freq, seed)
        norm += amp
        amp *= persistence
        freq *= lacunarity
    return total / norm

def fnum(n):
    s = "%.4f" % round(n, 4)
    return s.rstrip("0").rstrip(".") if "." in s else s

# ---------------------------------------------------------------- placement
class Placement:
    """Deterministic 2D AABB occupancy registry over the x/z plane.

    Tracks who occupies what so generators get collision-safe scatter for
    free. REJECTION POLICY: insert() RETURNS False on conflict (overlap or
    duplicate name) and leaves state untouched; call conflicts() to learn
    who blocks a box. Surfaces come in two flavors: GROUND (walkable=True)
    only provides top Y for snapping; SOLID (walkable=False) also rejects
    overlaps; blocks=True makes a standable platform that does both.
    Edge-touching boxes COUNT as overlapping - shrink one box when you
    truly want flush stacking. Iteration follows INSERTION order (plain
    dict, never a set), so results never depend on Python's hash
    randomization: same calls, same world, byte-identical output.

    Example:
        reg = worldkit.Placement()
        reg.insert("plaza", (-8, -8), (8, 8), top=0.2, walkable=True)  # ground
        if not reg.insert("well", (-1, -1), (1, 1), walkable=True,
                          blocks=True):
            print("blocked by:", reg.conflicts((-1, -1), (1, 1)))
        y = reg.ground_y(0, 0)            # -> 0.2 (plaza) under the well top
        for name in reg:                  # insertion order, deterministic
            ...
    """

    def __init__(self):
        # name -> [min_x, min_z, max_x, max_z, top_y, walkable, blocks]
        self._items = {}

    def insert(self, name, min_xy, max_xy, top=0.0, walkable=False,
               blocks=None):
        """Register a footprint. Returns True on success.

        Semantics: walkable=True entries are GROUND - they provide top Y to
        ground_y()/contains() but never reject overlaps (terrain, floor
        pads, decks). walkable=False entries are SOLID obstacles - they
        occupy space and reject overlapping inserts. blocks=True upgrades a
        walkable surface into a standable platform that BOTH provides top Y
        AND rejects overlaps. Default: blocks = not walkable.

        Returns False (no mutation) when `name` is already registered or a
        SOLID overlap exists. Reversed corners are normalized; non-finite
        bounds/top raise ValueError."""
        if name in self._items:
            return False
        w = bool(walkable)
        b = (not w) if blocks is None else bool(blocks)
        ax0, az0 = float(min_xy[0]), float(min_xy[1])
        ax1, az1 = float(max_xy[0]), float(max_xy[1])
        bx0, bx1 = (ax0, ax1) if ax0 <= ax1 else (ax1, ax0)
        bz0, bz1 = (az0, az1) if az0 <= az1 else (az1, az0)
        t = float(top)
        for v in (bx0, bx1, bz0, bz1, t):
            if not math.isfinite(v):
                raise ValueError("Placement.insert: non-finite bound/top for %r" % (name,))
        for o in self._items.values():
            if o[6] and bx0 <= o[2] and o[0] <= bx1 and bz0 <= o[3] and o[1] <= bz1:
                return False
        self._items[name] = [bx0, bz0, bx1, bz1, t, w, b]
        return True

    def conflicts(self, min_xy, max_xy, ignore=()):
        """Names of BLOCKING entries overlapping the query box, in
        insertion order (pure ground surfaces never conflict)."""
        ax0, az0 = float(min_xy[0]), float(min_xy[1])
        ax1, az1 = float(max_xy[0]), float(max_xy[1])
        bx0, bx1 = (ax0, ax1) if ax0 <= ax1 else (ax1, ax0)
        bz0, bz1 = (az0, az1) if az0 <= az1 else (az1, az0)
        out = []
        for nm, o in self._items.items():
            if not o[6] or nm in ignore:
                continue
            if bx0 <= o[2] and o[0] <= bx1 and bz0 <= o[3] and o[1] <= bz1:
                out.append(nm)
        return out

    def ground_y(self, x, z, default=0.0):
        """Ground-snap helper: bbox-top Y of the highest WALKABLE surface
        covering (x, z); `default` when no walkable surface covers it."""
        best, found = default, False
        for o in self._items.values():
            if not o[5]:
                continue
            if o[0] <= x <= o[2] and o[1] <= z <= o[3]:
                if not found or o[4] > best:
                    best, found = o[4], True
        return best

    def contains(self, x, z):
        """True when ANY registered walkable surface covers (x, z)."""
        for o in self._items.values():
            if o[5] and o[0] <= x <= o[2] and o[1] <= z <= o[3]:
                return True
        return False

    def bounds(self, name):
        """(min_xy, max_xy, top, walkable) snapshot of one entry."""
        o = self._items[name]
        return ((o[0], o[1]), (o[2], o[3]), o[4], o[5])

    def clone(self):
        """Faithful deterministic copy - safe to mutate for trial runs."""
        dup = Placement()
        dup._items = {k: list(v) for k, v in self._items.items()}
        return dup

    def names(self):
        """All registered names as a tuple, insertion order guaranteed."""
        return tuple(self._items)

    def __len__(self):
        return len(self._items)

    def __iter__(self):
        return iter(self._items)  # insertion order - deterministic by spec

    def __contains__(self, name):
        return name in self._items

def center_box(cx, cz, w, d):
    """AABB (min_xy, max_xy) from a center point + FULL width (x) / depth (z)."""
    return ((cx - w / 2.0, cz - d / 2.0), (cx + w / 2.0, cz + d / 2.0))

def reserve_spot(registry, name, cx, cz, w, d, lift=0.0, top=None,
                 walkable=False, blocks=None, y_default=0.0):
    """Collision-safe placement for one scene node.

    Queries BEFORE reserving: returns [x, y, z] ready to use as a
    write_scene() position, where y = registry.ground_y(x, z) + lift, or
    None when the footprint collides with anything already registered.
    On success the footprint itself is inserted (stored top defaults to the
    snapped y so stacked props can snap onto each other); walkable/blocks
    forward to Placement.insert (default: a solid obstacle).

    Example:
        pos = worldkit.reserve_spot(reg, "Chest_03", 4.5, -2.0, 1.0, 1.0)
        if pos is not None:
            placed.append(("Chest_03", pos, yaw, ["pickup"], "chest"))
    """
    mn, mx = center_box(cx, cz, w, d)
    if registry.conflicts(mn, mx):
        return None
    gy = registry.ground_y(float(cx), float(cz), y_default)
    y = gy + float(lift)
    registry.insert(name, mn, mx, top=(y if top is None else float(top)),
                    walkable=walkable, blocks=blocks)
    return [float(cx), y, float(cz)]

# -------------------------------------------------------------- mesh builder
class MeshBuilder:
    """Groups of triangles -> Wavefront OBJ. Cookbook sections 1-4."""

    def __init__(self):
        self.v, self.vn, self.groups = [], [], []
        self._cur = None

    def begin(self, name, material):
        self._cur = {"name": name, "mat": material, "faces": []}
        self.groups.append(self._cur)

    def _vi(self, p):
        # Copy the point: builder primitives share corner lists across many
        # faces, and a shared object would be shifted once per reference by
        # translate()/recenter_mesh(). Values (and thus OBJ bytes) unchanged.
        self.v.append([p[0], p[1], p[2]])
        return len(self.v)

    def tri(self, A, B, C):
        u = [B[i]-A[i] for i in range(3)]
        w = [C[i]-A[i] for i in range(3)]
        n = [u[1]*w[2]-u[2]*w[1], u[2]*w[0]-u[0]*w[2], u[0]*w[1]-u[1]*w[0]]
        l = math.sqrt(sum(c*c for c in n)) or 1.0
        self.vn.append([c/l for c in n])
        ni = len(self.vn)
        ia, ib, ic = self._vi(A), self._vi(B), self._vi(C)
        self._cur["faces"].append("f %d//%d %d//%d %d//%d" % (ia,ni,ib,ni,ic,ni))

    def quad(self, A, B, C, D):
        self.tri(A, B, C)
        self.tri(A, C, D)

    def translate(self, dx, dy, dz):
        """Shift every vertex by (dx, dy, dz). Normals are translation-
        invariant and stay untouched. Deterministic float ops; returns self
        so calls can chain. Use to move a prop mesh to origin AFTER building
        it offset (see recenter_mesh) - never to bake placement into verts."""
        for p in self.v:
            p[0] += dx
            p[1] += dy
            p[2] += dz
        return self

    def box(self, cx, cy, cz, hx, hy, hz):
        p = lambda sx, sy, sz: [cx+sx*hx, cy+sy*hy, cz+sz*hz]
        c = [p(1,-1,-1),p(1,-1,1),p(1,1,1),p(1,1,-1),
             p(-1,-1,-1),p(-1,-1,1),p(-1,1,1),p(-1,1,-1)]
        for q in ([0,3,2,1],[4,5,6,7],[2,3,7,6],[0,1,5,4],[1,2,6,5],[0,4,7,3]):
            self.quad(c[q[0]], c[q[1]], c[q[2]], c[q[3]])

    def roof_prism(self, cx, base_y, cz, rx, rz, rh):
        L0=[cx-rx,base_y,cz-rz]; L1=[cx-rx,base_y,cz+rz]
        R0=[cx+rx,base_y,cz-rz]; R1=[cx+rx,base_y,cz+rz]
        TB=[cx,base_y+rh,cz-rz]; TF=[cx,base_y+rh,cz+rz]
        self.quad(L0,L1,TF,TB)
        self.quad(R1,R0,TB,TF)
        self.tri(R1,TF,L1)
        self.tri(L0,TB,R0)

    def pyramid(self, cx, base_y, cz, hx, hz, h):
        apex = [cx, base_y+h, cz]
        loop = [[cx-hx,base_y,cz-hz],[cx+hx,base_y,cz-hz],
                [cx+hx,base_y,cz+hz],[cx-hx,base_y,cz+hz]]
        for i in range(4):
            self.tri(loop[i], apex, loop[(i+1)%4])

    def cyl(self, cx, y0, cz, r0, r1, h, seg=10, capped=True):
        """Cylinder; r1=0 gives a cone; equal radii with cap = disc."""
        apex_mode = r1 <= 1e-6
        rb = [[cx+r0*math.cos(2*math.pi*i/seg), y0,
               cz+r0*math.sin(2*math.pi*i/seg)] for i in range(seg)]
        rt = None if apex_mode else [
            [cx+r1*math.cos(2*math.pi*i/seg), y0+h,
             cz+r1*math.sin(2*math.pi*i/seg)] for i in range(seg)]
        ap = [cx, y0+h, cz]
        for i in range(seg):
            j = (i+1) % seg
            if apex_mode:
                self.tri(rb[i], ap, rb[j])
            else:
                self.quad(rb[i], rt[i], rt[j], rb[j])
                if capped:
                    self.tri([cx, y0+h, cz], rt[j], rt[i])
            if capped:
                self.tri([cx, y0, cz], rb[i], rb[j])

    def cone(self, cx, y0, cz, r, h, seg=10):
        self.cyl(cx, y0, cz, r, 0, h, seg)

    def octahedron(self, cx, cy, cz, r):
        """Two square pyramids base-to-base - pickups, crystals, stars."""
        T=[cx,cy+r,cz]; B=[cx,cy-r,cz]
        ring=[[cx+r,cy,cz],[cx,cy,cz+r],[cx-r,cy,cz],[cx,cy,cz-r]]
        for i in range(4):
            j=(i+1)%4
            self.tri(T, ring[j], ring[i])
            self.tri(B, ring[i], ring[j])

    def hex_tile(self, cx, y0, cz, r, h):
        """Hexagon prism - tabletop boards, strategy maps."""
        self.cyl(cx, y0, cz, r, r, h, seg=6)

    def sphere(self, cx, cy, cz, r, seg=10, rings=6):
        """UV-sphere - heads, eyes, organic blobs. Outward CCW faces."""
        rows = []
        for j in range(rings + 1):
            phi = math.pi * j / rings
            row = []
            for i in range(seg):
                th = 2 * math.pi * i / seg
                row.append([cx + r * math.sin(phi) * math.cos(th),
                            cy + r * math.cos(phi),
                            cz + r * math.sin(phi) * math.sin(th)])
            rows.append(row)
        pn = [cx, cy + r, cz]
        ps = [cx, cy - r, cz]
        for i in range(seg):
            k = (i + 1) % seg
            self.tri(rows[1][i], pn, rows[1][k])          # north fan
        for j in range(1, rings - 1):
            for i in range(seg):
                k = (i + 1) % seg
                self.quad(rows[j + 1][i], rows[j][i],
                          rows[j][k], rows[j + 1][k])
        for i in range(seg):
            k = (i + 1) % seg
            self.tri(rows[rings - 1][i], rows[rings - 1][k], ps)  # south fan

    def to_obj(self, name, mtllib):
        out = ["# litt worldgen asset - generated by math only",
               "# kit: template/tools/worldgen/",
               "mtllib %s.mtl" % mtllib, "o %s" % name]
        out += ["v %s %s %s" % (fnum(p[0]), fnum(p[1]), fnum(p[2])) for p in self.v]
        out += ["vn %s %s %s" % (fnum(n[0]), fnum(n[1]), fnum(n[2])) for n in self.vn]
        for g in self.groups:
            if not g["faces"]:
                continue  # never emit dead/empty groups (bare usemtl breaks parsers)
            out.append("g %s" % g["name"])
            if g["mat"]:
                out.append("usemtl %s" % g["mat"])
            out += g["faces"]
        tris = sum(len(g["faces"]) for g in self.groups)
        return NL.join(out) + NL, len(self.v), tris

# ------------------------------------------------- transform convention
class TransformError(ValueError):
    """Raised when a mesh violates the origin-centered prop convention."""

_AXES = {"x": (0,), "y": (1,), "z": (2,),
         "xz": (0, 2), "xy": (0, 1), "yz": (1, 2), "xyz": (0, 1, 2)}

def mesh_centroid(mesh):
    """Mean vertex position [cx, cy, cz]. Accepts a MeshBuilder or any
    iterable of [x, y, z] rows. Raises ValueError on zero vertices."""
    verts = mesh.v if hasattr(mesh, "v") else list(mesh)
    n = len(verts)
    if not n:
        raise ValueError("mesh_centroid: mesh has no vertices")
    sx = sy = sz = 0.0
    for p in verts:
        sx += p[0]
        sy += p[1]
        sz += p[2]
    return [sx / n, sy / n, sz / n]

def assert_origin_centered(mesh, tol=ORIGIN_TOL, axes="xz"):
    """Convention guard: True when the vertex centroid sits within `tol`
    meters of origin on the chosen axes, else raises TransformError.

    axes="xz" (default) matches the base-center prop convention - x/z must
    center on 0 while y may ride up the mesh (a 2 m tall crate built from
    y=0 has centroid y=1 and is CORRECT). Use "xyz" for meshes whose whole
    bounding volume must hug the origin.

    Example:
        worldkit.assert_origin_centered(mb)          # raises if misplaced
        ok = worldkit.mesh_centroid(mb)[0] < tol     # manual variant
    """
    if axes not in _AXES:
        raise ValueError("assert_origin_centered: unknown axes %r" % (axes,))
    c = mesh_centroid(mesh)
    bad = [(i, c[i]) for i in _AXES[axes] if abs(c[i]) > tol]
    if bad:
        detail = ", ".join("%s=%.4f" % ("xyz"[i], v) for i, v in bad)
        raise TransformError(
            "%s not origin-centered (%s): centroid [%s], %s exceeds tol %.3f"
            % (type(mesh).__name__, axes,
               ", ".join("%.4f" % v for v in c), detail, float(tol)))
    return True

def recenter_mesh(mb, axes="xz"):
    """Repair helper: translate a MeshBuilder so its vertex centroid hits
    origin on the chosen axes (default x/z only - base height stays
    meaningful). Returns the applied (dx, dy, dz) offset."""
    if axes not in _AXES:
        raise ValueError("recenter_mesh: unknown axes %r" % (axes,))
    c = mesh_centroid(mb)
    off = [0.0, 0.0, 0.0]
    for i in _AXES[axes]:
        off[i] = -c[i]
    mb.translate(off[0], off[1], off[2])
    return tuple(off)

# ------------------------------------------------------------------- output
def write_mtl(path, materials):
    chunks = []
    for name, col in sorted(materials.items()):
        r, g, b = col
        chunks.append(NL.join(["newmtl %s" % name,
                               "Ka 1.000 1.000 1.000",
                               "Kd %.3f %.3f %.3f" % (r, g, b),
                               "Ks 0.050 0.050 0.050", "Ns 8.0"]))
    path.write_text((NL * 2).join(chunks) + NL, encoding="utf-8")

def emit_chunk(mb, mat, cx, cz, size, res, seed, height_fn, band_fn=None):
    """One seamless terrain chunk; heights sampled in world space.
    band_fn(tri) -> material name; triangles are bucketed so each material
    becomes exactly ONE group (no duplicate headers).
    NOTE - sanctioned exception to the transform convention: chunk vertices
    are WORLD-space BY DESIGN (neighboring chunks must share edge vertices),
    so a chunk node MUST sit at identity position [0, *, 0]. Never offset
    both the vertices and the node, or consumers double-transform it."""
    ox, oz, step = cx*size, cz*size, size/res
    grid = {}
    for j in range(res + 1):
        for i in range(res + 1):
            wx, wz = ox + i*step, oz + j*step
            grid[(i, j)] = [wx, height_fn(wx, wz, seed), wz]
    buckets = {}
    for j in range(res):
        for i in range(res):
            p00,p10,p11,p01 = grid[(i,j)],grid[(i+1,j)],grid[(i+1,j+1)],grid[(i,j+1)]
            for t in ((p00,p01,p11),(p00,p11,p10)):
                chosen = band_fn(t) if band_fn else mat
                buckets.setdefault(chosen, []).append(t)
    for mname, tris in buckets.items():
        mb.begin(mname, mname)
        for t in tris:
            mb.tri(*t)

# ------------------------------------------------------- project plumbing
def register_index(assets_dir, aid, rel_path, loader="litt_asset::manager::AssetManager::load_model", kind="model"):
    idx_path = Path(assets_dir) / "asset_index.json"
    data = None
    if idx_path.exists():
        try: data = json.loads(idx_path.read_text(encoding="utf-8"))
        except Exception: data = None
    if not isinstance(data, dict) or "assets" not in data:
        data = {"format": "litt-asset-index", "version": 1,
                "description": "Machine-readable asset manifest.", "assets": []}
    data["assets"] = [e for e in data.get("assets", []) if e.get("id") != aid]
    data["assets"].append({"id": aid, "type": kind, "path": rel_path, "loader": loader})
    idx_path.parent.mkdir(parents=True, exist_ok=True)
    idx_path.write_text(json.dumps(data, indent=2) + NL, encoding="utf-8")

def write_scene(path, placed, title, placement=None):
    """placed: list of (node_name, position[x,y,z], yaw_degrees, tags_list)
              or (node_name, position, yaw, tags, model_ref).
              Optionally a 6th element: footprint = (half_w, half_d), the
              x/z HALF-extents of the node's solid body (same convention as
              MeshBuilder hx/hz).

    Every node gets a `model:<ref>` tag so runtimes can instantiate it:
    explicit ref when provided (a falsy ref falls back to the snake-cased
    node name, `Coin_07` -> `model:coin_07`). Missing meshes are skipped
    safely by consumers.

    TRANSFORM CONVENTION: meshes are modeled at origin; `position` carries
    the whole placement. Baking world coords into vertices AND setting
    position double-transforms the prop (~2x displacement) - see module
    docstring.

    PLACEMENT REGISTRY: pass placement=reg (a Placement) to hard-validate
    every footprinted node against the registry AND against earlier nodes
    of this batch. Any conflict raises ValueError BEFORE the file is
    written (scene on disk stays intact); otherwise all footprints commit
    into the registry so later scenes/writers can query ground_y/conflicts.
    Idempotent by name: a node already registered (e.g. pre-tracked via
    reserve_spot) passes iff its x/z bounds are unchanged; a moved or
    duplicated name raises instead of silently double-booking.

    Example:
        reg = worldkit.Placement()
        reg.insert("deck", (-6, -2), (6, 2), top=0.5, walkable=True)
        pos = worldkit.reserve_spot(reg, "Coin_01", 3.0, 0.0, 0.8, 0.8, lift=1.2)
        placed = [("Coin_01", pos, 0, ["pickup", "score"], "coin", (0.4, 0.4))]
        worldkit.write_scene(out / "assets/scenes/world.lscn.json",
                             placed, "demo", placement=reg)
    """
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    q = lambda deg: [0.0, round(math.sin(math.radians(deg)/2), 4), 0.0, round(math.cos(math.radians(deg)/2), 4)]
    nodes = [{"name": "Root", "id": 0, "parent": None,
              "children": list(range(1, len(placed)+1)),
              "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1],
              "visible": True, "layer": 0, "tags": []}]
    for n, item in enumerate(placed, start=1):
        nm, pos, yaw, tags = item[:4]
        tags = list(tags)
        ref = item[4] if len(item) > 4 and item[4] else nm.lower().replace(" ", "_")
        mt = "model:" + ref
        if not any(t.startswith("model:") for t in tags):
            tags.append(mt)
        nodes.append({"name": nm, "id": n, "parent": 0, "children": [],
                      "position": [float(pos[0]), float(pos[1]), float(pos[2])],
                      "rotation": q(yaw), "scale": [1,1,1],
                      "visible": True, "layer": 0, "tags": tags})
    scene = {"format": "litt-scene", "version": 1, "root_id": 0,
             "next_id": len(placed)+1, "nodes": nodes}
    if placement is not None:
        # Validate on a trial clone so a conflict never half-commits into
        # the caller's registry; only when the whole batch is clean do we
        # commit the new names back.
        trial = placement.clone()
        fps = []
        for item in placed:
            if len(item) > 5 and item[5] is not None:
                hw, hd = float(item[5][0]), float(item[5][1])
                mn, mx = center_box(float(item[1][0]), float(item[1][2]),
                                    2.0 * hw, 2.0 * hd)
                fps.append((item[0], mn, mx))
        errs = []
        for nm, mn, mx in fps:
            if nm in trial:
                # Idempotent re-write: reserve_spot() already tracks this
                # node. Identical x/z bounds -> already handled; anything
                # else means one name claims two footprints -> refuse.
                b = trial.bounds(nm)
                if b[0] != mn or b[1] != mx:
                    errs.append("node %r already registered at (%s, %s) - "
                                "refuses to double-book"
                                % (nm, b[0], b[1]))
                continue
            if not trial.insert(nm, mn, mx):
                blockers = trial.conflicts(mn, mx) or ["(duplicate name)"]
                errs.append("node %r footprint blocked by: %s"
                            % (nm, ", ".join(blockers)))
        if errs:
            raise ValueError("write_scene(%s): placement conflicts\n  %s"
                             % (path.name, "\n  ".join(errs)))
        for nm, mn, mx in fps:
            if nm not in placement:
                b = trial.bounds(nm)
                placement.insert(nm, b[0], b[1], top=b[2], walkable=b[3])
    path.write_text(json.dumps(scene, indent=2) + NL, encoding="utf-8")

def write_state(path, payload):
    """Call LAST - viewers poll this file."""
    Path(path).write_text(json.dumps(payload, indent=2) + NL, encoding="utf-8")

def append_log(path, agent, prompt, headline, bullets):
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    body = NL.join(["---", "", "## %s - ACTION by %s (AI)" % (stamp, agent),
                    "- prompt: %s" % (prompt or "(autonomous generation)"),
                    "- action: %s" % headline] +
                   ["- " + b for b in bullets] + [""])
    p = Path(path); p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "a", encoding="utf-8") as fh:
        fh.write(body)

def sha12(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()[:12]

def save_prop(models_dir, name, mb, mtllib, mats, assets_dir=None,
              enforce_origin=False, auto_recenter=False,
              origin_axes="xz", origin_tol=ORIGIN_TOL):
    """Write one prop OBJ(+shared MTL handled by caller), return (path,kb,tris).

    TRANSFORM CONVENTION HOOK - props are modeled AT ORIGIN (x/z centered
    on 0, base resting on y=0) and placed purely via scene node.position:
        mb = worldkit.MeshBuilder(); mb.begin("crate", "crate_wood")
        mb.box(0, 0.25, 0, 0.25, 0.25, 0.25)          # built at origin
        worldkit.save_prop(out / "models", "crate", mb, "materials", mats,
                           assets_dir=out / "assets", enforce_origin=True)
        # ...later, scene side: position carries the placement only.
    enforce_origin=True raises TransformError when the vertex centroid is
    off-origin (catches the baked-coords double-transform bug class before
    it ships). auto_recenter=True instead repairs: re-centers x/z and writes
    (y untouched so base height survives). The two are mutually exclusive;
    both default False so legacy world-baked writers keep working until they
    opt in."""
    if enforce_origin and auto_recenter:
        raise ValueError("save_prop(%s): choose enforce_origin or "
                         "auto_recenter, not both" % name)
    if auto_recenter:
        recenter_mesh(mb, origin_axes)
    elif enforce_origin:
        assert_origin_centered(mb, origin_tol, origin_axes)
    models_dir = Path(models_dir); models_dir.mkdir(parents=True, exist_ok=True)
    obj_text, nv, nf = mb.to_obj(name, mtllib)
    p = models_dir / (name + ".obj")
    p.write_text(obj_text, encoding="utf-8")
    kb = p.stat().st_size / 1024.0
    if kb > MODEL_BUDGET_KB:
        raise SystemExit("OVER BUDGET: %s = %.1f KB" % (name, kb))
    if assets_dir is not None:
        register_index(assets_dir, name, "models/%s.obj" % name)
    return p, kb, nf

def write_mtl_for(models_dir, mtllib, mats):
    write_mtl(Path(models_dir) / (mtllib + ".mtl"), mats)

# ------------------------------------------------------------------ themes
def load_theme(name):
    """Fetch a theme dict {palette, props, env_notes} from themes.json."""
    p = Path(__file__).parent / "themes.json"
    data = json.loads(p.read_text(encoding="utf-8"))
    if name not in data["themes"]:
        raise SystemExit("unknown theme %r - available: %s" % (name, ", ".join(sorted(data["themes"]))))
    return data["themes"][name]

def list_themes():
    p = Path(__file__).parent / "themes.json"
    return sorted(json.loads(p.read_text(encoding="utf-8"))["themes"].keys())