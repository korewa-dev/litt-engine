#!/usr/bin/env python3
"""Litt Live - perpetual chunked landscape generator.

Generates endless grass terrain as square chunks around the origin.
Heights are sampled in WORLD space, so neighboring chunks share edge
vertices exactly - chunks tile seamlessly and the world can grow forever
by raising --radius. Fully deterministic: same seed, same world.

Usage (from anywhere):
  python live_landscape.py --radius 2 --seed 1337 \
      --agent ox-alpha --prompt "Create a perpetual landscape with endless grass."

Idempotent expansion: existing chunk files are kept, only missing ones are
generated. world_state.json and scenes/world.lscn.json are rewritten to
cover the full radius. LIVE_LOG.md gets one appended entry per run.
"""
import argparse, datetime, hashlib, json, math, sys
from pathlib import Path

NL = chr(10)
LIVE_DIR = Path(__file__).resolve().parent.parent
MODELS = LIVE_DIR / "assets" / "models"
STATE = LIVE_DIR / "world_state.json"
SCENE_DIR = LIVE_DIR / "scenes"
LOG = LIVE_DIR / "LIVE_LOG.md"
INDEX = LIVE_DIR / "assets" / "asset_index.json"

CHUNK_DEFAULT = 16.0
RES_DEFAULT = 12
AMP = 1.6
FREQ = 0.08

MATS = {
    "grass_a":  (0.435, 0.627, 0.329),
    "grass_b":  (0.525, 0.725, 0.420),
    "dirt":     (0.541, 0.435, 0.278),
}

# ------------------------------------------------------------- noise (cookbook)
def _hash(ix, iy, seed):
    n = (ix * 73856093) ^ (iy * 19349663) ^ (seed * 126271)
    n &= 0xFFFFFFFF
    return n / 0xFFFFFFFF

def value_noise(x, y, seed):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    u, v = fx*fx*(3-2*fx), fy*fy*(3-2*fy)
    a=_hash(ix,iy,seed); b=_hash(ix+1,iy,seed); c=_hash(ix,iy+1,seed); d=_hash(ix+1,iy+1,seed)
    return a*(1-u)*(1-v)+b*u*(1-v)+c*(1-u)*v+d*u*v

def fbm(x, y, seed, octaves=4):
    amp, freq, total, norm = 1.0, 1.0, 0.0, 0.0
    for o in range(octaves):
        total += amp * value_noise(x*freq, y*freq, seed+o*101)
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return total / norm

# ------------------------------------------------------------------ OBJ output
def fnum(n):
    s = "%.4f" % round(n, 4)
    return s.rstrip("0").rstrip(".") if "." in s else s

def gen_chunk(cx, cz, size, res, seed):
    """One terrain chunk; world-space sampling guarantees seamless tiling."""
    ox, oz = cx*size, cz*size
    step = size/res
    verts, faces = [], []
    def h(wx, wz):
        return fbm(wx*FREQ, wz*FREQ, seed) * AMP
    grid = {}
    for j in range(res+1):
        for i in range(res+1):
            wx, wz = ox + i*step, oz + j*step
            grid[(i,j)] = [wx, h(wx,wz), wz]
    def emit_tri(A,B,C,mat_faces):
        u=[B[k]-A[k] for k in range(3)]; w=[C[k]-A[k] for k in range(3)]
        n=[u[1]*w[2]-u[2]*w[1], u[2]*w[0]-u[0]*w[2], u[0]*w[1]-u[1]*w[0]]
        l=math.sqrt(sum(c*c for c in n)) or 1.0
        ni=len(verts)+0  # normals appended 1:1 with faces below
        for P in (A,B,C):
            verts.append((P,n))
        mat_faces.append((len(verts)-2, len(verts)-1, len(verts), ni+1))
    buckets = {m: [] for m in MATS}
    for j in range(res):
        for i in range(res):
            p00,p10,p11,p01 = grid[(i,j)],grid[(i+1,j)],grid[(i+1,j+1)],grid[(i,j+1)]
            for tri in ((p00,p01,p11),(p00,p11,p10)):
                mx = sum(p[0] for p in tri)/3; mz = sum(p[2] for p in tri)/3
                my = sum(p[1] for p in tri)/3
                if fbm(mx*0.15, mz*0.15, seed+555, 3) > 0.74:
                    m = "dirt"
                else:
                    m = "grass_a" if value_noise(mx*0.5, mz*0.5, seed+77) > 0.5 else "grass_b"
                buckets[m].append(tri)
    lines = ["# litt live chunk - perpetual grass world",
             "# chunk (%d,%d) seed=%d size=%g res=%d" % (cx,cz,seed,size,res),
             "mtllib materials.mtl", "o chunk_%d_%d" % (cx,cz)]
    vcount = 0
    for m, tris in buckets.items():
        if not tris:
            continue
        lines.append("g %s" % m); lines.append("usemtl %s" % m)
        for (A,B,C) in tris:
            u=[B[k]-A[k] for k in range(3)]; w=[C[k]-A[k] for k in range(3)]
            n=[u[1]*w[2]-u[2]*w[1], u[2]*w[0]-u[0]*w[2], u[0]*w[1]-u[1]*w[0]]
            l=math.sqrt(sum(c*c for c in n)) or 1.0
            idxs=[]
            for P in (A,B,C):
                vcount += 1
                lines.append("v %s %s %s" % (fnum(P[0]), fnum(P[1]), fnum(P[2])))
                lines.append("vn %s %s %s" % (fnum(n[0]/l), fnum(n[1]/l), fnum(n[2]/l)))
                idxs.append(vcount)
            lines.append("f %d//%d %d//%d %d//%d" % (idxs[0],idxs[0],idxs[1],idxs[1],idxs[2],idxs[2]))
    return NL.join(lines) + NL, len(buckets["grass_a"]+buckets["grass_b"]+buckets["dirt"])

def write_mtl():
    chunks=[]
    for name,(r,g,b) in MATS.items():
        chunks.append(NL.join(["newmtl %s"%name, "Ka 1.000 1.000 1.000",
            "Kd %.3f %.3f %.3f"%(r,g,b), "Ks 0.050 0.050 0.050", "Ns 8.0"]))
    (MODELS/"materials.mtl").write_text((NL*2).join(chunks)+NL, encoding="utf-8")

# ------------------------------------------------------------------ registry
def update_index(chunk_ids):
    data = None
    if INDEX.exists():
        try: data = json.loads(INDEX.read_text(encoding="utf-8"))
        except Exception: data = None
    if not isinstance(data, dict) or "assets" not in data:
        data = {"format":"litt-asset-index","version":1,
                "description":"example-village-style manifest for the live world","assets":[]}
    have = {e.get("id") for e in data["assets"]}
    for cid, path in chunk_ids:
        if cid in have: continue
        data["assets"].append({"id":cid,"type":"model","path":path,
            "loader":"litt_asset::manager::AssetManager::load_model"})
    INDEX.parent.mkdir(parents=True, exist_ok=True)
    INDEX.write_text(json.dumps(data, indent=2)+NL, encoding="utf-8")

def write_scene(chunk_ids, size):
    SCENE_DIR.mkdir(parents=True, exist_ok=True)
    nodes = [{"name":"Root","id":0,"parent":None,
              "children":list(range(1,len(chunk_ids)+1)),
              "position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],
              "visible":True,"layer":0,"tags":[]}]
    for n,(cid,path) in enumerate(chunk_ids, start=1):
        cx,cz = cid.replace("chunk_","").split("_")
        nodes.append({"name":cid,"id":n,"parent":0,"children":[],
            "position":[int(cx)*size,0.0,int(cz)*size],
            "rotation":[0,0,0,1],"scale":[1,1,1],"visible":True,"layer":0,
            "tags":["live","terrain"]})
    scene = {"format":"litt-scene","version":1,"root_id":0,
             "next_id":len(chunk_ids)+1,"nodes":nodes}
    (SCENE_DIR/"world.lscn.json").write_text(json.dumps(scene,indent=2)+NL, encoding="utf-8")

def append_log(agent, prompt, made, radius, seed):
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    entry = NL.join([
        "---", "",
        "## %s - ACTION by %s (AI)" % (stamp, agent),
        "- prompt: %s" % (prompt or "(autonomous expansion)"),
        "- action: perpetual grass landscape -> radius %d (seed %d)" % (radius, seed),
        "- generated this run: %d chunk(s) %s" % (len(made), ", ".join(made[:6]) + (" ..." if len(made)>6 else "")),
        "- state: world_state.json + scenes/world.lscn.json rewritten; viewers may reload",
        ""])
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(entry)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--radius", type=int, default=2)
    ap.add_argument("--seed", type=int, default=1337)
    ap.add_argument("--chunk-size", type=float, default=CHUNK_DEFAULT)
    ap.add_argument("--res", type=int, default=RES_DEFAULT)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    MODELS.mkdir(parents=True, exist_ok=True)
    write_mtl()
    coords = [(x,z) for x in range(-a.radius, a.radius+1)
                     for z in range(-a.radius, a.radius+1)]
    made, registry = [], []
    for (x,z) in coords:
        cid = "chunk_%d_%d" % (x,z)
        fname = cid + ".obj"
        fpath = MODELS / fname
        registry.append((cid, "models/"+fname))
        if fpath.exists():
            continue
        obj_text, tris = gen_chunk(x, z, a.chunk_size, a.res, a.seed)
        fpath.write_text(obj_text, encoding="utf-8")
        kb = fpath.stat().st_size/1024.0
        made.append(fname)
        print("[live] +%s (%d tris, %.1f KB)" % (fname, tris, kb))
    update_index(registry)
    write_scene(registry, a.chunk_size)
    state = {
        "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
        "updated": datetime.datetime.now().isoformat(timespec="seconds"),
        "seed": a.seed, "chunk_size": a.chunk_size, "radius": a.radius,
        "camera": {"target": [0, 0, 0], "distance": 46},
        "chunks": [{"id": cid, "path": "assets/"+p,
                    "position": [int(cid.split("_")[1])*a.chunk_size, 0,
                                 int(cid.split("_")[2])*a.chunk_size]}
                   for cid, p in registry],
        "palette": {k: v for k, v in MATS.items()},
    }
    STATE.write_text(json.dumps(state, indent=2)+NL, encoding="utf-8")
    append_log(a.agent, a.prompt, made, a.radius, a.seed)
    print("[live] world: %d chunks (radius %d) | state + scene + index updated" % (len(registry), a.radius))
    print("[live] log entry appended to LIVE_LOG.md")

if __name__ == "__main__":
    main()
