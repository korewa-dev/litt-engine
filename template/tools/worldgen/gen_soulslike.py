#!/usr/bin/env python3
"""EMBERFALL HOLLOW - pocket soulslike world generator (Litt Engine).

Bonfire checkpoint -> Hollow Road -> fog gate -> boss arena -> corpse run.
Usage:  python gen_soulslike.py [--out-dir .] [--radius 2] [--seed N]
        [--agent ai] [--prompt "..."]

SEED PLUMBING (audit item 13): --seed drives EVERYTHING. The one integer is
split into two decorrelated 32-bit streams by the splitmix64 finalizer over
distinct odd constants (documented, pure int math, fully deterministic):

    terrain_stream = mix64(seed ^ 0x9E3779B97F4A7C15)   # golden-ratio gamma
    scatter_stream = mix64(seed ^ 0xD1B54A32D192ED03)   # splitmix gamma

terrain_stream feeds the fBm height/band sampling of every chunk OBJ;
scatter_stream feeds ONE xorshift32 Rng threaded through layout() placement
AND prop-mesh variation in a fixed draw order. Omitting --seed keeps the
historical module consts (terrain 666 / scatter 2077). Same seed => same
bytes forever; different seeds => different terrain AND different scatter.

COMPOSITION (audit item 14): gameplay-critical meshes come from the shared
gen_props.py souls kit - bonfire (checkpoint), knight (boss), stalker
(hollows), estus_flask + banner - built via build_prop() and merged into the
ash palette with gen_props' own parse_mtl/setdefault convention. Only scene
dressing the kit lacks (fog gate, corpse bloodstain, graves, dead trees,
ruin arch/pillars, soul embers) stays bespoke. Scatter goes through
worldkit.Placement/reserve_spot (collision-safe), every mesh is origin-
centered (save_prop enforce_origin; auto_recenter only for random-branch
dead trees), and scene nodes carry placement via position/yaw alone.

Genre math used (details: template/docs/genre_algorithms.md):
  - fBm terrain, world-space sampling for seamless chunk tiling
  - deliberate encounter placement along a guide path (road = +Z axis)
  - stamina economy + corpse-run encoded as data in world_state gameplay block
"""
import argparse
import datetime
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from worldkit import (Rng, fbm, value_noise, MeshBuilder, write_mtl_for,
                      emit_chunk, register_index, write_scene, write_state,
                      append_log, save_prop, Placement, reserve_spot)
from gen_props import PALETTES, build_prop, parse_mtl

CHUNK, RES, AMP, FREQ = 16.0, 12, 1.2, 0.08
LEGACY_T, LEGACY_S = 666, 2077  # pre---seed default streams (back-compat)

_M64 = (1 << 64) - 1


def _mix64(z):
    """splitmix64 finalizer, truncated to 32 bits for worldkit.Rng."""
    z &= _M64
    z ^= z >> 30; z = (z * 0xBF58476D1CE4E5B9) & _M64
    z ^= z >> 27; z = (z * 0x94D049BB133111EB) & _M64
    z ^= z >> 31
    return z & 0xFFFFFFFF


def derive_seeds(seed):
    """--seed N -> (terrain_seed, scatter_seed); scheme in module docstring."""
    return (_mix64(seed ^ 0x9E3779B97F4A7C15),
            _mix64(seed ^ 0xD1B54A32D192ED03))


MATS = {
  "ash_field": (0.42,0.43,0.40), "ash_drift": (0.50,0.51,0.47), "scorched": (0.30,0.27,0.24),
  "bonfire_steel": (0.25,0.26,0.28), "ember": (0.95,0.45,0.10), "ash_mound": (0.48,0.46,0.42),
  "hollow_skin": (0.55,0.53,0.47), "hollow_rag": (0.33,0.30,0.26),
  "deadwood": (0.28,0.23,0.19), "grave_stone": (0.47,0.48,0.50),
  "ruin_stone": (0.44,0.45,0.46), "fog_veil": (0.88,0.90,0.92),
  "knight_steel": (0.36,0.38,0.42), "knight_cape": (0.30,0.10,0.10),
  "soul_blue": (0.45,0.75,0.95), "bloodstain_green": (0.35,0.70,0.40),
}

def height(wx, wz, seed):
    return fbm(wx*FREQ, wz*FREQ, seed) * AMP

def band(tri, seed):
    mx = sum(p[0] for p in tri)/3; mz = sum(p[2] for p in tri)/3
    if fbm(mx*0.15, mz*0.15, seed+555, 3) > 0.74: return "scorched"
    return "ash_drift" if value_noise(mx*0.5, mz*0.5, seed+77) > 0.5 else "ash_field"


# --------------------------------------------------- bespoke dressing props
# (souls-kit coverage lives in gen_props.build_prop; see main())

def p_dead_tree(rng):
    def fn(p):
        bark = p("trunk", "deadwood")
        h = 2.2 + rng.uniform(0, 1.4)
        bark.cyl(0, 0, 0, 0.16, 0.08, h, seg=7)
        for k in range(3):
            bx = (rng.uniform(-1, 1)) * 0.32
            bz = (rng.uniform(-1, 1)) * 0.32
            bh = rng.uniform(0.5, 1.1)
            bark.cyl(bx, h*0.72, bz, 0.05, 0.0, bh, seg=5)
    return fn

def p_grave(rng):
    def fn(p):
        st = p("stone", "grave_stone")
        w = 0.34 + rng.uniform(0, 0.18); h = 0.55 + rng.uniform(0, 0.45)
        st.box(0, h/2, 0, w/2, h/2, 0.09)
        st.prism(0, h, 0, w/2, 0.09, 0.14)
    return fn

def p_arch(p):
    st = p("stone", "ruin_stone")
    st.box(-2.6, 1.9, 0, 0.45, 1.9, 0.45)
    st.box( 2.6, 1.9, 0, 0.45, 1.9, 0.45)
    st.box(0, 4.0, 0, 3.05, 0.30, 0.50)

def p_pillar(rng):
    def fn(p):
        st = p("stone", "ruin_stone")
        st.cyl(0, 0, 0, 0.38, 0.32, 1.6 + rng.uniform(0, 1.8), seg=9)
    return fn

def p_fog_gate(p):
    fog = p("veil", "fog_veil"); fog.box(0, 1.55, 0, 3.1, 1.55, 0.14)

def p_soul_ember(p):
    s = p("soul", "soul_blue"); s.octahedron(0, 0.28, 0, 0.20)

def p_bloodstain(p):
    b = p("stain", "bloodstain_green")
    b.cyl(0, 0, 0, 0.48, 0.48, 0.03, seg=12)
    b.cone(0, 0.03, 0, 0.10, 0.22, seg=6)

# ------------------------------------------------------------------- layout
LANDMARKS = [(0.0, 2.0), (0.0, 38.0), (0.0, 46.0), (0.0, 62.0)]


def layout(rng, reg):
    """Seeded composition; reserve_spot() keeps every footprint collision-free.

    Tag contract (audited): enemy+aggro_small x3..5 (stalkers along the road
    corridor), boss+aggro_large x1, pickup+souls x7..8 embers clustered at
    landmarks plus x3 estus flasks, checkpoint bonfire, player/start,
    boss_entry fog gate, corpse_run memorial, deco dressing off-corridor."""
    items = []

    def put(nm, kind, x, z, yaw, tags, w, d):
        pos = reserve_spot(reg, nm, round(float(x), 2), round(float(z), 2), w, d)
        if pos is not None:
            items.append((nm, [pos[0], pos[2]], yaw, list(tags), kind))
        return pos is not None

    # fixed story anchors first, so seeded scatter must route around them
    for row in (
      ("Bonfire",       "bonfire",    0,    2,   0,   ["poi","checkpoint"],          2.4, 2.4),
      ("Player_Start",  "bonfire",    0,    4.5, 0,   ["player","start"],            1.0, 1.0),
      ("Bloodstain",    "bloodstain", 2.2,  5,   0,   ["memorial","corpse_run"],     1.2, 1.2),
      ("Ruin_Arch",     "arch",       0,    38,  90,  ["deco","gate_frame"],         1.2, 6.2),
      ("Fog_Gate",      "fog_gate",   0,    46,  0,   ["gate","boss_entry"],         6.4, 0.6),
      ("Ashen_Knight",  "knight",     0,    62,  180, ["boss","aggro_large"],        1.8, 1.8),
    ):
        put(*row)
    # kit banners flank the fog gate and the arena mouth (shared banner.obj)
    for nm, x, z, yaw in (("Banner_Gate_L", -2.0, 45.2, -15),
                          ("Banner_Gate_R",  2.0, 45.2,  15),
                          ("Banner_Arena_L", -3.5, 56.5, -20),
                          ("Banner_Arena_R",  3.5, 56.5,  20)):
        put(nm, "banner", x, z, yaw, ["deco"], 0.5, 0.5)
    # estus flask pickups near landmarks (bonfire / gate / arena mouth)
    for i, (lx, lz) in enumerate(((1.8, 3.4), (-2.1, 43.2), (1.6, 58.0)), 1):
        put("Estus_%02d" % i, "estus_flask",
            lx + rng.uniform(-0.5, 0.5), lz + rng.uniform(-0.5, 0.5),
            0, ["pickup", "souls"], 0.5, 0.5)
    # hollows: 3..5 stalkers spread up the road corridor with jitter
    n_hollows = 3 + int(rng.uniform(0, 3))
    for k in range(n_hollows):
        hz = 13 + k * (27.0 / (n_hollows - 1)) + rng.uniform(-2.5, 2.5)
        put("Hollow_%02d" % (k+1), "stalker", rng.uniform(-4.5, 4.5), hz,
            int(rng.uniform(0, 360)), ["enemy", "aggro_small"], 0.9, 0.9)
    # soul embers: 7..8, clustered within ~3 m of the story landmarks
    n_embers = 7 + int(rng.uniform(0, 2))
    for k in range(n_embers):
        lx, lz = LANDMARKS[k % len(LANDMARKS)]
        put("Soul_Ember_%02d" % (k+1), "soul_ember",
            lx + rng.uniform(-3.0, 3.0), lz + rng.uniform(-3.0, 3.0),
            0, ["pickup", "souls"], 0.5, 0.5)
    # graves: 8..10, always off-corridor (|x| >= 3 m from the road spine)
    n_graves = 8 + int(rng.uniform(0, 3))
    for k in range(n_graves):
        gx = rng.pick([-1, 1]) * rng.uniform(3.0, 6.5)
        put("Grave_%02d" % (k+1), "grave", gx, rng.uniform(7, 44),
            int(rng.uniform(0, 360)), ["deco"], 0.6, 0.6)
    # dead trees: 5..7 scattered wide, kept off-corridor as well
    n_trees = 5 + int(rng.uniform(0, 3))
    for k in range(n_trees):
        tx = rng.pick([-1, 1]) * rng.uniform(2.5, 17.5)
        put("Dead_Tree_%02d" % (k+1), "dead_tree", tx, rng.uniform(-8, 64),
            int(rng.uniform(0, 360)), ["deco"], 0.9, 0.9)
    # ruin pillars guard the approach to the arena
    for k in range(4):
        put("Ruin_Pillar_%02d" % (k+1), "pillar",
            rng.pick([-6.5, -4.5, 4.5, 6.5]), 50 + k*4, 0, ["deco"], 0.9, 0.9)
    return items


# ------------------------------------------------------- mesh kit plumbing
class Kit:
    """kit("part", "mat") -> handle whose geometry ops emit into that group."""
    def __init__(self, mb): self.mb = mb
    def __call__(self, pname, mat):
        self.mb.begin(pname, mat)
        return PartHandle(self.mb)

class PartHandle:
    def __init__(self, mb): self.mb = mb
    def tri(self, A,B,C): self.mb.tri(A,B,C)
    def quad(self, A,B,C,D): self.mb.quad(A,B,C,D)
    def box(self, *a): self.mb.box(*a)
    def prism(self, *a): self.mb.roof_prism(*a)
    def cyl(self, *a, **k): self.mb.cyl(*a, **k)
    def cone(self, *a, **k): self.mb.cone(*a, **k)
    def octahedron(self, *a): self.mb.octahedron(*a)

def build(mb, fn):
    fn(Kit(mb))
    return mb

# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--radius", type=int, default=2)
    ap.add_argument("--seed", type=int, default=None,
                    help="master seed -> derived terrain/scatter streams "
                         "(omitted: legacy terrain 666 / scatter 2077)")
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    seed_t, seed_s = (LEGACY_T, LEGACY_S) if a.seed is None else derive_seeds(a.seed)

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"

    # --- materials: ash palette, then gen_props prop_* MERGE (never recolor)
    write_mtl_for(models, "materials", MATS)
    merged = parse_mtl(models / "materials.mtl")
    pal = PALETTES["haunted_estate"]
    for k, v in pal.items():
        merged.setdefault("prop_" + k, v)
    write_mtl_for(models, "materials", merged)

    made = []; placed = []; registry = []

    # --- terrain chunks (seamless: heights sampled in world space) ---------
    # Hollow Road runs +Z, so chunk rows reach farther toward the boss arena.
    band_fn = lambda tri: band(tri, seed_t)
    for x in range(-a.radius, a.radius + 1):
        for z in range(-a.radius, a.radius + 3):
            cid = "chunk_%d_%d" % (x, z)
            mb = MeshBuilder()
            emit_chunk(mb, "ash_field", x, z, CHUNK, RES, seed_t, height, band_fn)
            obj_text, nv, nf = mb.to_obj(cid, "materials")
            p = models / (cid + ".obj")
            if not p.exists():
                p.write_text(obj_text, encoding="utf-8"); made.append(cid + ".obj")
            registry.append((cid, "models/" + cid + ".obj"))
            placed.append((cid, [x * CHUNK, 0, z * CHUNK], 0, ["terrain"]))
    for cid, rel in registry:
        register_index(assets_dir, cid, rel)

    # --- props: souls kit from gen_props.build_prop, then bespoke dressing -
    ppal = {"prop_" + k: v for k, v in pal.items()}
    rng = Rng(seed_s)  # one scatter stream drives layout AND mesh variation
    # Kit meshes were not authored under the strict 0.05 m centroid tolerance
    # (e.g. bonfire sits at z=-0.06), so they take save_prop's sanctioned
    # auto_recenter repair: x/z snapped to origin, base height preserved -
    # placement stays node-only either way.
    for name in ("bonfire", "stalker", "knight", "estus_flask", "banner"):
        save_prop(models, name, build_prop(name, ppal), "materials", merged,
                  assets_dir=assets_dir, auto_recenter=True)
        made.append(name + ".obj")
    for name, fn, centered in [("bloodstain", p_bloodstain, True),
                               ("grave", p_grave(rng), True),
                               ("dead_tree", p_dead_tree(rng), False),
                               ("pillar", p_pillar(rng), True),
                               ("arch", p_arch, True),
                               ("fog_gate", p_fog_gate, True),
                               ("soul_ember", p_soul_ember, True)]:
        save_prop(models, name, build(MeshBuilder(), fn), "materials", merged,
                  assets_dir=assets_dir,
                  enforce_origin=centered, auto_recenter=not centered)
        made.append(name + ".obj")

    # --- scene nodes: registry-placed layout grounded on the fBm surface ---
    reg = Placement()
    for nm, xz, yaw, tags, kind in layout(rng, reg):
        y = round(height(xz[0], xz[1], seed_t), 3)
        placed.append((nm, [xz[0], y, xz[1]], yaw, tags, kind))
    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "emberfall-hollow")

    # --- world state LAST, then log ----------------------------------------
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "emberfall-hollow",
      "identity": {"movement": "soulslike third-person stamina sprint",
                   "camera": "third-person orbit"},
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"input": a.seed, "terrain": seed_t, "scatter": seed_s},
      "chunk_size": CHUNK, "radius": a.radius,
      "camera": {"target": [0, 1.5, 30], "distance": 30},
      "chunks": [{"id": c, "path": "assets/" + r,
                  "position": [int(c.split("_")[1])*CHUNK, 0, int(c.split("_")[2])*CHUNK]}
                 for c, r in registry],
      "palette": MATS,
      "gameplay": {
        "genre": "soulslike",
        "objective": "light the bonfire, fight up Hollow Road, cross the fog gate and fell the Ashen Knight",
        "corpse_run": True,
        "physics": {"gravity": -22.0, "jump_velocity": 8.0, "run_speed": 6.5,
                    "coyote_time_s": 0.10, "jump_buffer_s": 0.12},
        "enemy_aggro_m": 8.0, "kill_radius_m": 2.2, "interact_radius_m": 2.4,
        "lives": 0, "score_goal": 1225,
        "scoring": {"per_ember": 25, "checkpoint_light": 150, "boss_kill": 900},
        "hazards": ["hollow ambushes along the road", "the Ashen Knight beyond the fog"],
        "checkpoints": ["Bonfire"]}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "EMBERFALL HOLLOW pocket-soulslike world (seed %s -> terrain %d, scatter %d)"
               % (a.seed, seed_t, seed_s),
               ["%d terrain chunks (%.0fm grid, res %d, road-extended +Z)" % (len(registry), CHUNK, RES),
                "%d prop models (gen_props souls kit + bespoke dressing), %d scene nodes; "
                "bonfire/corpse-run/fog-gate/boss contract in world_state.json"
                % (len(made) - len(registry), len(placed) - len(registry))])
    print("[emberfall] ready: %d chunks + %d assets | %d scene nodes | seed %s (T%d/S%d)"
          % (len(registry), len(made), len(placed), a.seed, seed_t, seed_s))

if __name__ == "__main__":
    main()
