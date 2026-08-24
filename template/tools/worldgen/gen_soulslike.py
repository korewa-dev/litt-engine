#!/usr/bin/env python3
"""EMBERFALL HOLLOW - pocket soulslike world generator (Litt Engine).

Bonfire checkpoint -> Hollow Road -> fog gate -> boss arena -> corpse run.
Usage:  python gen_soulslike.py [--out-dir .] [--radius 2] [--agent ai] [--prompt "..."]

Genre math used (details: template/docs/genre_algorithms.md):
  - fBm terrain, world-space sampling for seamless chunk tiling
  - deliberate encounter placement along a guide path (road = +Z axis)
  - stamina economy + corpse-run encoded as data in world_state gameplay block
"""
import argparse
import datetime
import math
from pathlib import Path

from worldkit import (Rng, fbm, value_noise, MeshBuilder, write_mtl_for,
                      emit_chunk, register_index, write_scene, write_state,
                      append_log, save_prop, sha12, NL)

CHUNK, RES, AMP, FREQ = 16.0, 12, 1.2, 0.08
SEED_T, SEED_S = 666, 2077

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


def p_bonfire(p):
    ash = p("mound", "ash_mound");   ash.cyl(0, 0, 0, 0.95, 0.70, 0.22)
    steel = p("sword", "bonfire_steel")
    steel.box(0, 0.85, 0, 0.055, 0.75, 0.02)
    steel.box(0, 1.38, 0, 0.30, 0.035, 0.04)
    steel.box(0, 1.52, 0, 0.06, 0.06, 0.06)
    emb = p("coals", "ember");       emb.cyl(0, 0.20, 0, 0.34, 0.30, 0.10)
    stone = p("ring", "grave_stone")
    for i in range(6):
        a = 6.2831853 * i / 6
        stone.box(1.15*math.cos(a), 0.09, 1.15*math.sin(a), 0.14, 0.09, 0.14)

def p_hollow(p):
    skin = p("body", "hollow_skin"); rag = p("rag", "hollow_rag")
    rag.box(0, 0.28, 0, 0.30, 0.28, 0.22)
    skin.box(0.03, 0.78, 0.05, 0.24, 0.26, 0.19)
    skin.box(0.10, 1.06, 0.10, 0.11, 0.11, 0.11)
    skin.box(-0.34, 0.62, 0.02, 0.07, 0.34, 0.07)
    skin.box(0.38, 0.58, -0.02, 0.07, 0.30, 0.07)

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

def p_boss_knight(p):
    steel = p("armor", "knight_steel"); cape = p("cape", "knight_cape")
    steel.box(-0.45, 0.85, 0, 0.20, 0.85, 0.24)
    steel.box( 0.45, 0.85, 0, 0.20, 0.85, 0.24)
    steel.box(0, 1.95, 0, 0.62, 0.55, 0.40)
    steel.box(0, 2.68, 0, 0.30, 0.28, 0.30)
    cape.quad([-0.55,1.65,-0.28],[0.55,1.65,-0.28],[0.42,0.15,-0.34],[-0.42,0.15,-0.34])
    steel.box(0.92, 1.9, 0.30, 0.06, 1.35, 0.10)
    steel.box(0.92, 3.28, 0.30, 0.16, 0.05, 0.16)

def p_soul_ember(p):
    s = p("soul", "soul_blue"); s.octahedron(0, 0.28, 0, 0.20)

def p_bloodstain(p):
    b = p("stain", "bloodstain_green")
    b.cyl(0, 0, 0, 0.48, 0.48, 0.03, seg=12)
    b.cone(0, 0.03, 0, 0.10, 0.22, seg=6)

# ------------------------------------------------------------------- layout
def layout():
    rng = Rng(SEED_S)
    items = [
      ("Bonfire",      "bonfire",      [0, 0, 2],    0,   ["poi","checkpoint"]),
      ("Player_Start", "bonfire",      [0, 0, 4.5],  0,   ["player","start"]),
      ("Bloodstain",   "bloodstain",   [2.2, 0, 5],  0,   ["memorial","corpse_run"]),
      ("Hollow_01",    "hollow",       [-4, 0, 14],  15,  ["enemy","aggro_small"]),
      ("Hollow_02",    "hollow",       [5, 0, 22],   -20, ["enemy","aggro_small"]),
      ("Hollow_03",    "hollow",       [-2, 0, 31],  40,  ["enemy","aggro_small"]),
      ("Fog_Gate",     "fog_gate",     [0, 0, 46],   0,   ["gate","boss_entry"]),
      ("Ashen_Knight", "boss_knight",  [0, 0, 62],   180, ["boss","aggro_large"]),
    ]
    for k in range(7):
        ex = rng.uniform(-14, 14); ez = rng.uniform(6, 58)
        items.append(("Soul_Ember_%02d" % (k+1), "soul_ember", [round(ex,2), 0, round(ez,2)], 0, ["pickup","souls"]))
    for k in range(9):
        gx = rng.uniform(3.0, 5.8) * rng.pick([-1, 1])
        gz = rng.uniform(7, 44)
        items.append(("Grave_%02d" % (k+1), "grave", [round(gx,2), 0, round(gz,2)],
                      int(rng.uniform(0, 360)), ["deco"]))
    for k in range(6):
        tx = rng.uniform(-18, 18); tz = rng.uniform(-8, 66)
        items.append(("Dead_Tree_%02d" % (k+1), "dead_tree", [round(tx,2), 0, round(tz,2)],
                      int(rng.uniform(0, 360)), ["deco"]))
    for k in range(4):
        px = rng.pick([-6.5, -4.5, 4.5, 6.5])
        items.append(("Ruin_Pillar_%02d" % (k+1), "pillar", [px, 0, 50 + k*4], 0, ["deco"]))
    items.append(("Ruin_Arch", "arch", [0, 0, 38], 90, ["deco", "gate_frame"]))
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
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)
    made = []; placed = []; registry = []

    # --- terrain chunks (seamless: heights sampled in world space) ---------
    # Hollow Road runs +Z, so chunk rows reach farther toward the boss arena.
    band_fn = lambda tri: band(tri, SEED_T)
    for x in range(-a.radius, a.radius + 1):
        for z in range(-a.radius, a.radius + 3):
            cid = "chunk_%d_%d" % (x, z)
            mb = MeshBuilder()
            emit_chunk(mb, "ash_field", x, z, CHUNK, RES, SEED_T, height, band_fn)
            obj_text, nv, nf = mb.to_obj(cid, "materials")
            p = models / (cid + ".obj")
            if not p.exists():
                p.write_text(obj_text, encoding="utf-8"); made.append(cid + ".obj")
            registry.append((cid, "models/" + cid + ".obj"))
            placed.append((cid, [x * CHUNK, 0, z * CHUNK], 0, ["terrain"]))
    for cid, rel in registry:
        register_index(assets_dir, cid, rel)

    # --- props (each unique model built once, shared by its scene nodes) ---
    rng = Rng(SEED_S)
    for name, fn in [("bonfire", p_bonfire), ("bloodstain", p_bloodstain),
                     ("hollow", p_hollow), ("grave", p_grave(rng)),
                     ("dead_tree", p_dead_tree(rng)), ("pillar", p_pillar(rng)),
                     ("arch", p_arch), ("fog_gate", p_fog_gate),
                     ("boss_knight", p_boss_knight), ("soul_ember", p_soul_ember)]:
        save_prop(models, name, build(MeshBuilder(), fn), "materials", MATS, assets_dir)
        made.append(name + ".obj")

    # --- scene nodes from layout(), each prop grounded on the fBm surface ---
    for nm, kind, pos, yaw, tags in layout():
        y = round(height(pos[0], pos[2], SEED_T), 3)
        placed.append((nm, [pos[0], y, pos[2]], yaw, list(tags), kind))
    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "emberfall-hollow")

    # --- world state LAST, then log ----------------------------------------
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "emberfall-hollow",
      "identity": {"movement": "soulslike third-person stamina sprint",
                   "camera": "third-person orbit"},
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"terrain": SEED_T, "scatter": SEED_S},
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
               "EMBERFALL HOLLOW pocket-soulslike world (terrain seed %d, scatter seed %d)" % (SEED_T, SEED_S),
               ["%d terrain chunks (%.0fm grid, res %d, road-extended +Z)" % (len(registry), CHUNK, RES),
                "%d prop models, %d scene nodes; bonfire/corpse-run/fog-gate/boss contract in world_state.json"
                % (len(made) - len(registry), len(placed))])
    print("[emberfall] ready: %d chunks + %d assets | %d scene nodes"
          % (len(registry), len(made), len(placed)))

if __name__ == "__main__":
    main()
...