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

# ------------------------------------------------------------------ prop kit
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
    for k in range(10):
        tx = rng.uniform(-16, 16); tz = rng.uniform(4, 60)
        if abs(tx) < 3.5 and tz < 46: tx += 6 * (1 if tx >= 0 else -1)
        items.append(("Dead_Tree_%02d" % (k+1), "dead_tree", [round(tx,2), 0, round(tz,2)], int(rng.uniform(0,360)), ["decor"]))
    for k in range(8):
        gx = rng.pick([-1, 1]) * rng.uniform(2.2, 5.5); gz = 6 + k * 5.5
        items.append(("Grave_%02d" % (k+1), "gravestone", [round(gx,2), 0, round(gz,2)], int(rng.uniform(-25,25)), ["decor","story"]))
    for k in range(8):
        a = 6.2831853 * k / 8
        items.append(("Arena_Pillar_%02d" % (k+1), "broken_pillar",
                      [round(11*math.sin(a),2), 0, round(62+11*math.cos(a),2)], int(k*45), ["arena","decor"]))
    items.append(("Ruin_Arch", "arch", [0, 0, 38], 0, ["decor","story"]))
    return items

# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--radius", type=int, default=2)
    ap.add_argument("--seed-terrain", type=int, default=SEED_T)
    ap.add_argument("--seed-scatter", type=int, default=SEED_S)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)

    made = []
    coords = [(x, z) for x in range(-a.radius, a.radius+1)
                       for z in range(-a.radius, a.radius+1)]
    registry = []
    for (x, z) in coords:
        cid = "chunk_%d_%d" % (x, z)
        mb = MeshBuilder()
        emit_chunk(mb, "ash_field", x, z, CHUNK, RES, a.seed_terrain, height, lambda t: band(t, a.seed_terrain))
        obj_text, nv, nf = mb.to_obj(cid, "materials")
        p = models / (cid + ".obj")
        if not p.exists():
            p.write_text(obj_text, encoding="utf-8"); made.append(cid + ".obj")
        registry.append((cid, "models/" + cid + ".obj"))
    for cid, rel in registry:
        register_index(assets_dir, cid, rel)

    props = [
      ("bonfire", p_bonfire), ("hollow", p_hollow),
      ("dead_tree", p_dead_tree(Rng(SEED_S))), ("gravestone", p_grave(Rng(SEED_S+1))),
      ("arch", p_arch), ("broken_pillar", p_pillar(Rng(SEED_S+2))),
      ("fog_gate", p_fog_gate), ("boss_knight", p_boss_knight),
      ("soul_ember", p_soul_ember), ("bloodstain", p_bloodstain),
    ]
    class PartHandle:
        def __init__(self, mb): self.mb = mb
        def tri(self, A,B,C): self.mb.tri(A,B,C)
        def quad(self, A,B,C,D): self.mb.quad(A,B,C,D)
        def box(self, *a): self.mb.box(*a)
        def prism(self, *a): self.mb.roof_prism(*a)
        def pyramid(self, *a): self.mb.pyramid(*a)
        def cyl(self, *a, **k): self.mb.cyl(*a, **k)
        def cone(self, *a, **k): self.mb.cone(*a, **k)
        def octahedron(self, *a): self.mb.octahedron(*a)
    class Kit:
        """p("part_name", "material") -> handle emitting into that group."""
        def __init__(self, mb): self.mb = mb
        def __call__(self, pname, mat):
            self.mb.begin(pname, mat)
            return PartHandle(self.mb)
    prop_names = []
    for name, fn in props:
        mb = MeshBuilder()
        fn(Kit(mb))
        p, kb, nf = save_prop(models, name, mb, "materials", MATS, assets_dir)
        prop_names.append(name)
        print("[emberfall] +%s.obj (%d tris, %.1f KB)" % (name, nf, kb))

    placed = []
    for nm, model, pos, yaw, tags in layout():
        placed.append((nm, pos, yaw, tags + ["model:" + model]))
    for cid, rel in registry:
        cx, cz = cid.replace("chunk_", "").split("_")
        placed.append((cid, [int(cx)*CHUNK, 0, int(cz)*CHUNK], 0, ["terrain"]))
    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "emberfall-hollow")

    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "emberfall-hollow",
      "updated": __import__("datetime").datetime.now().isoformat(timespec="seconds"),
      "seed": {"terrain": a.seed_terrain, "scatter": a.seed_scatter},
      "chunk_size": CHUNK, "radius": a.radius,
      "camera": {"target": [0, 1, 2], "distance": 20},
      "chunks": [{"id": c, "path": "assets/" + r,
                  "position": [int(c.split("_")[1])*CHUNK, 0, int(c.split("_")[2])*CHUNK]}
                 for c, r in registry],
      "palette": MATS,
      "gameplay": {
        "genre": "soulslike",
        "objective": "Emberfall Hollow: kindle the bonfire, survive the road, face the Ashen Knight",
        "enemy_aggro_m": 8.0,
        "corpse_run": True,
        "spawn": [0.0, 0.0, 4.5],
        "checkpoint": {"node": "Bonfire", "respawn_on_death": True},
        "souls": {"pickups": 7, "corpse_run": "return to Bloodstain node to recover"},
        "combat": {"stamina_max": 100, "roll_cost": 25, "attack_cost": 20, "regen_per_sec": 35},
        "enemies": {"hollow_aggro_radius_m": 6, "boss": "Ashen Knight"},
        "fog_gate": {"position": [0, 1.55, 46], "opens_on": "approach"}
      }
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "EMBERFALL HOLLOW soulslike world -> radius %d (terrain seed %d, scatter seed %d)" % (a.radius, a.seed_terrain, a.seed_scatter),
               ["re-themed %d terrain chunks to ash palette" % len(registry),
                "props built: " + ", ".join(prop_names),
                "scene nodes: %d (bonfire, 3 hollows, fog gate, boss arena, embers, graves)" % len(placed),
                "gameplay spec written into world_state.json"])
    print("[emberfall] world ready: %d chunks + %d prop types | state + scene + index updated" % (len(registry), len(prop_names)))

if __name__ == "__main__":
    main()