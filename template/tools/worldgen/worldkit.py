#!/usr/bin/env python3
"""WorldKit - shared engine for Litt genre world generators.

Every gen_*.py script next to this file imports WorldKit and produces a
complete playable-style world folder: meshes + materials + asset index +
scene file + world state + log entry. Deterministic: same seeds, same bytes.

Conventions: meters, Y-up, right-handed, CCW winding, origin at base center.
Algorithms reference: template/docs/procedural_asset_math.md

Custom game with no preset generator? Copy gen_custom.py and edit the TODOs.
"""
import datetime
import hashlib
import json
import math
from pathlib import Path

NL = chr(10)
MODEL_BUDGET_KB = 500

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
        self.v.append(p)
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
    becomes exactly ONE group (no duplicate headers)."""
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

def write_scene(path, placed, title):
    """placed: list of (node_name, position[x,y,z], yaw_degrees, tags_list)
              or (node_name, position, yaw, tags, model_ref).

    Every node gets a `model:<ref>` tag so runtimes can instantiate it:
    explicit ref when provided, otherwise the snake-cased node name
    (`Coin_07` -> `model:coin_07`). Missing meshes are skipped safely by
    consumers."""
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    q = lambda deg: [0.0, round(math.sin(math.radians(deg)/2), 4), 0.0, round(math.cos(math.radians(deg)/2), 4)]
    nodes = [{"name": "Root", "id": 0, "parent": None,
              "children": list(range(1, len(placed)+1)),
              "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1],
              "visible": True, "layer": 0, "tags": []}]
    for n, item in enumerate(placed, start=1):
        nm, pos, yaw, tags = item[:4]
        tags = list(tags)
        ref = item[4] if len(item) > 4 else nm.lower().replace(" ", "_")
        mt = "model:" + ref
        if not any(t.startswith("model:") for t in tags):
            tags.append(mt)
        nodes.append({"name": nm, "id": n, "parent": 0, "children": [],
                      "position": [float(pos[0]), float(pos[1]), float(pos[2])],
                      "rotation": q(yaw), "scale": [1,1,1],
                      "visible": True, "layer": 0, "tags": tags})
    scene = {"format": "litt-scene", "version": 1, "root_id": 0,
             "next_id": len(placed)+1, "nodes": nodes}
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

def save_prop(models_dir, name, mb, mtllib, mats, assets_dir=None):
    """Write one prop OBJ(+shared MTL handled by caller), return (path,kb,tris)."""
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