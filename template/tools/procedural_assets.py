#!/usr/bin/env python3
"""Litt Engine - procedural asset generator (reference implementation).

Implements the algorithms documented in template/docs/procedural_asset_math.md.
Any AI agent: run it as-is, or copy these functions and change parameters.
Stdlib only, Python 3.8+.

Conventions: meters, Y-up right-handed, CCW winding, origin at base center.

Usage:
  python procedural_assets.py <house|cottage|tree|crate|terrain> --name my_asset
                             [--out-dir assets] [--seed 7]
                             [--width W --depth D --height H --ridge R]
                             [--no-register]

Every successful run prints a SHA-256 and registers the asset in
<out-dir>/asset_index.json. Then add a provenance row to your ATTRIBUTION.md
(method: procedural, license: CC0-generated).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

MODEL_BUDGET_KB = 500
NL = chr(10)

# ---------------------------------------------------------------- vector math
def sub(a, b): return [a[0]-b[0], a[1]-b[1], a[2]-b[2]]

def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def fnum(n):
    s = "%.4f" % round(n, 4)
    return s.rstrip("0").rstrip(".") if "." in s else s

# ------------------------------------------------------- deterministic RNG
class Rng:
    """xorshift32 - same seed, same asset, forever. Cookbook section 7."""
    def __init__(self, seed=7):
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

# ------------------------------------------------------------ value noise / fbm
def _lattice_hash(ix, iy, seed):
    n = (ix * 73856093) ^ (iy * 19349663) ^ (seed * 126271)
    n &= 0xFFFFFFFF
    return n / 0xFFFFFFFF

def value_noise(x, y, seed):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    u = fx * fx * (3 - 2 * fx)
    v = fy * fy * (3 - 2 * fy)
    a = _lattice_hash(ix, iy, seed)
    b = _lattice_hash(ix + 1, iy, seed)
    c = _lattice_hash(ix, iy + 1, seed)
    d = _lattice_hash(ix + 1, iy + 1, seed)
    return a*(1-u)*(1-v) + b*u*(1-v) + c*(1-u)*v + d*u*v

def fbm(x, y, seed, octaves=4, persistence=0.5, lacunarity=2.0):
    amp, freq, total, norm = 1.0, 1.0, 0.0, 0.0
    for _ in range(octaves):
        total += amp * value_noise(x*freq, y*freq, seed)
        norm += amp
        amp *= persistence
        freq *= lacunarity
    return total / norm

# --------------------------------------------------------------- mesh builder
class MeshBuilder:
    """Accumulates groups of faces -> Wavefront OBJ. Cookbook sections 1-4."""

    def __init__(self):
        self.v, self.vn, self.groups = [], [], []
        self._cur = None

    def begin(self, name, material):
        self._cur = {"name": name, "mat": material, "faces": []}
        self.groups.append(self._cur)

    def _vi(self, p):
        self.v.append(p)
        return len(self.v)

    def tri(self, A, B, C):
        n = cross(sub(B, A), sub(C, A))
        l = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2) or 1.0
        self.vn.append([n[0]/l, n[1]/l, n[2]/l])
        ni = len(self.vn)
        self._cur["faces"].append(
            "f %d//%d %d//%d %d//%d" % (self._vi(A), ni, self._vi(B), ni, self._vi(C), ni))

    def quad(self, A, B, C, D):
        self.tri(A, B, C)
        self.tri(A, C, D)

    # ---- cookbook section 1
    def box(self, cx, cy, cz, hx, hy, hz):
        p = lambda sx, sy, sz: [cx+sx*hx, cy+sy*hy, cz+sz*hz]
        c = [p(1,-1,-1), p(1,-1,1), p(1,1,1), p(1,1,-1),
             p(-1,-1,-1), p(-1,-1,1), p(-1,1,1), p(-1,1,-1)]
        for qi in ([0,3,2,1], [4,5,6,7], [2,3,7,6],
                   [0,1,5,4], [1,2,6,5], [0,4,7,3]):
            a, bq, c2, d = (c[i] for i in qi)
            self.quad(a, bq, c2, d)

    # ---- cookbook section 2
    def roof_prism(self, cx, base_y, cz, rx, rz, rh):
        L0=[cx-rx,base_y,cz-rz]; L1=[cx-rx,base_y,cz+rz]
        R0=[cx+rx,base_y,cz-rz]; R1=[cx+rx,base_y,cz+rz]
        TB=[cx,base_y+rh,cz-rz]; TF=[cx,base_y+rh,cz+rz]
        self.quad(L0, L1, TF, TB)
        self.quad(R1, R0, TB, TF)
        self.tri(R1, TF, L1)
        self.tri(L0, TB, R0)

    # ---- cookbook section 3
    def pyramid(self, cx, base_y, cz, hx, hz, h):
        apex = [cx, base_y+h, cz]
        loop = [[cx-hx,base_y,cz-hz], [cx+hx,base_y,cz-hz],
                [cx+hx,base_y,cz+hz], [cx-hx,base_y,cz+hz]]
        for i in range(4):          # base loop CCW from above, fan to apex
            self.tri(loop[i], apex, loop[(i+1) % 4])

    # ---- cookbook section 4
    def cylinder(self, cx, y0, cz, r0, r1, h, segments=10, capped=True):
        top_is_apex = r1 <= 1e-6
        ring_b = [[cx + r0*math.cos(2*math.pi*i/segments), y0,
                   cz + r0*math.sin(2*math.pi*i/segments)] for i in range(segments)]
        ring_t = None if top_is_apex else [
            [cx + r1*math.cos(2*math.pi*i/segments), y0+h,
             cz + r1*math.sin(2*math.pi*i/segments)] for i in range(segments)]
        apex = [cx, y0+h, cz]
        for i in range(segments):
            j = (i + 1) % segments
            if top_is_apex:
                self.tri(ring_b[i], apex, ring_b[j])   # cone side
            else:
                self.quad(ring_b[i], ring_t[i], ring_t[j], ring_b[j])
            if capped:
                if not top_is_apex:
                    self.tri(apex if False else [cx, y0+h, cz], ring_t[j], ring_t[i])
                self.tri([cx, y0, cz], ring_b[i], ring_b[j])

    # ---- OBJ emission (v/vn first, then grouped faces)
    def to_obj(self, name, mtllib):
        out = ["# litt engine procedural asset - generated by math only",
               "# generator: template/tools/procedural_assets.py",
               "mtllib %s.mtl" % mtllib, "o %s" % name]
        out += ["v %s %s %s" % (fnum(p[0]), fnum(p[1]), fnum(p[2])) for p in self.v]
        out += ["vn %s %s %s" % (fnum(n[0]), fnum(n[1]), fnum(n[2])) for n in self.vn]
        for g in self.groups:
            out.append("g %s" % g["name"])
            out.append("usemtl %s" % g["mat"])
            out += g["faces"]
        tris = sum(len(g["faces"]) for g in self.groups)
        return NL.join(out) + NL, len(self.v), tris

def write_mtl(path, materials):
    chunks = []
    for name, col in materials.items():
        r, g, bcol = col
        chunks.append(NL.join([
            "newmtl %s" % name,
            "Ka 1.000 1.000 1.000",
            "Kd %.3f %.3f %.3f" % (r, g, bcol),
            "Ks 0.100 0.100 0.100",
            "Ns 10.0"]))
    path.write_text((NL * 2).join(chunks) + NL, encoding="utf-8")

# ------------------------------------------------------------------- presets
def build_house(b, w, d, h, ridge):
    hw, hd = w / 2.0, d / 2.0
    b.begin("walls", "walls");   b.box(0, h/2, 0, hw, h/2, hd)
    b.begin("roof", "roof");     b.roof_prism(0, h, 0, hw+0.30, hd+0.30, ridge)
    b.begin("door", "door");     b.box(0, 1.05, hd+0.03, 0.45, 1.05, 0.05)
    b.begin("windows", "windows")
    wx = max(hw - 0.85, 0.6)
    for sx in (-1, 1):
        b.box(sx*wx, 1.8, hd+0.03, 0.5, 0.5, 0.05)
        b.box(sx*(hw+0.03), 1.8, 0, 0.05, 0.5, 0.65)
    b.begin("chimney", "chimney")
    b.box(hw*0.45, h + ridge*0.45, hd*0.35, 0.26, 0.75, 0.26)

HOUSE_MATS   = {"walls": (0.85,0.78,0.63), "roof": (0.66,0.29,0.22), "door": (0.42,0.29,0.18),
                "windows": (0.62,0.83,0.91), "chimney": (0.50,0.48,0.46)}
COTTAGE_MATS = {"walls": (0.92,0.88,0.80), "roof": (0.25,0.35,0.55), "door": (0.30,0.22,0.14),
                "windows": (0.62,0.83,0.91), "chimney": (0.50,0.48,0.46)}
TREE_MATS    = {"bark": (0.36,0.25,0.16), "foliage": (0.22,0.48,0.28)}
CRATE_MATS   = {"wood": (0.55,0.38,0.22)}
TERRAIN_MATS = {"ground": (0.33,0.52,0.30)}

def preset_tree(b, seed):
    rng = Rng(seed)
    b.begin("bark", "bark")
    b.cylinder(0, 0, 0, 0.16, 0.12, 1.1, segments=8)
    b.begin("foliage", "foliage")
    y, layers, r = 0.95, 2 + int(rng.uniform(0, 2)), 1.05
    for _ in range(layers):
        hh = 1.5 - 0.0
        hh = 1.5 - (y - 0.95) * 0.35
        b.pyramid(0, y, 0, r, r, hh)
        y += hh * 0.55
        r *= 0.72

def preset_crate(b, size=0.85):
    b.begin("wood", "wood")
    b.box(0, size/2, 0, size/2, size/2, size/2)

def preset_terrain(b, size, res, max_h, seed):
    """Cookbook section 5: grid res x res, heights from fbm(seed)."""
    b.begin("ground", "ground")
    grid = {}
    for j in range(res + 1):
        for i in range(res + 1):
            x = (i / res - 0.5) * size
            z = (j / res - 0.5) * size
            grid[(i, j)] = [x, fbm(i/res*3, j/res*3, seed) * max_h, z]
    for j in range(res):
        for i in range(res):
            p00, p10 = grid[(i, j)], grid[(i+1, j)]
            p11, p01 = grid[(i+1, j+1)], grid[(i, j+1)]
            b.tri(p00, p01, p11)
            b.tri(p00, p11, p10)

PRESETS = {
    "house":   dict(mats=HOUSE_MATS,   fn=lambda b, a: build_house(b, a.width, a.depth, a.height, a.ridge)),
    "cottage": dict(mats=COTTAGE_MATS, fn=lambda b, a: build_house(b, min(a.width,3.6), min(a.depth,4.4), min(a.height,2.8), min(a.ridge,1.6))),
    "tree":    dict(mats=TREE_MATS,    fn=lambda b, a: preset_tree(b, a.seed)),
    "crate":   dict(mats=CRATE_MATS,   fn=lambda b, a: preset_crate(b)),
    "terrain": dict(mats=TERRAIN_MATS, fn=lambda b, a: preset_terrain(b, max(a.width,16), 24, max(a.height,1.5), a.seed)),
}

# -------------------------------------------------------------- registration
INDEX_SCAFFOLD = {
    "format": "litt-asset-index", "version": 1,
    "description": "Machine-readable asset manifest. Agents: read this file to discover assets instead of scanning the tree.",
    "assets": [],
}

def register_index(assets_dir, aid, rel_path):
    idx_path = Path(assets_dir) / "asset_index.json"
    data = json.loads(idx_path.read_text(encoding="utf-8")) if idx_path.exists() else dict(INDEX_SCAFFOLD)
    entry = {"id": aid, "type": "model", "path": rel_path,
             "loader": "litt_asset::manager::AssetManager::load_model"}
    kept = [e for e in data.get("assets", []) if e.get("id") != aid]
    data["assets"] = kept + [entry]
    idx_path.write_text(json.dumps(data, indent=2) + NL, encoding="utf-8")
    return idx_path

# ---------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description="Litt procedural asset generator")
    sub = ap.add_subparsers(dest="preset", required=True)
    for name in PRESETS:
        sp = sub.add_parser(name)
        sp.add_argument("--name", required=True)
        sp.add_argument("--out-dir", default="assets")
        sp.add_argument("--seed", type=int, default=7)
        sp.add_argument("--width", type=float, default=4.0)
        sp.add_argument("--depth", type=float, default=5.0)
        sp.add_argument("--height", type=float, default=3.0)
        sp.add_argument("--ridge", type=float, default=1.8)
        sp.add_argument("--no-register", action="store_true")
    a = ap.parse_args(argv)

    builder = MeshBuilder()
    spec = PRESETS[a.preset]
    spec["fn"](builder, a)

    models_dir = Path(a.out_dir) / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    obj_text, nv, nf = builder.to_obj(a.name, a.name)
    obj_path = models_dir / (a.name + ".obj")
    obj_path.write_text(obj_text, encoding="utf-8")
    write_mtl(models_dir / (a.name + ".mtl"), spec["mats"])

    kb = obj_path.stat().st_size / 1024.0
    sha = hashlib.sha256(obj_path.read_bytes()).hexdigest()[:12]
    tag = "<= %dKB budget" % MODEL_BUDGET_KB if kb <= MODEL_BUDGET_KB else "OVER BUDGET!"
    print("[ok] wrote %s (%d verts, %d tris, %.1f KB %s)" % (obj_path, nv, nf, kb, tag))
    print("     sha256:%s  mats: %s" % (sha, ", ".join(sorted(spec["mats"]))))
    if kb > MODEL_BUDGET_KB:
        sys.exit(1)
    if not a.no_register:
        ip = register_index(Path(a.out_dir), a.name, "models/%s.obj" % a.name)
        print("[ok] registered id=%s -> %s" % (a.name, ip))
    print("next: add ATTRIBUTION.md row (method: procedural, license: CC0-generated)")

if __name__ == "__main__":
    main()
