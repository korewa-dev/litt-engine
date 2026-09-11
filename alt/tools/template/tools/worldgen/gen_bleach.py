#!/usr/bin/env python3
"""BLEACH: SOUL REAPER - soulslike world generator (Litt Engine).

You are a Shinigami. Wield your Zanpakuto. Purify the Hollows.
Fight through the Spirit World to the Menos Grande's lair.

Usage:  python gen_bleach.py [--out-dir .] [--radius 2] [--seed N]
        [--agent ai] [--prompt "..."]

Genre: soulslike | Kit: souls | Theme: spirit_world
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

CHUNK, RES, AMP, FREQ = 16.0, 12, 1.0, 0.07

_M64 = (1 << 64) - 1


def _mix64(z):
    z &= _M64
    z ^= z >> 30; z = (z * 0xBF58476D1CE4E5B9) & _M64
    z ^= z >> 27; z = (z * 0x94D049BB133111EB) & _M64
    z ^= z >> 31
    return z & 0xFFFFFFFF


def derive_seeds(seed):
    return (_mix64(seed ^ 0x9E3779B97F4A7C15),
            _mix64(seed ^ 0xD1B54A32D192ED03))


# BLEACH Spirit World palette
MATS = {
    "spirit_ground": (0.15, 0.20, 0.35),
    "spirit_drift": (0.20, 0.28, 0.45),
    "spirit_mist": (0.30, 0.40, 0.55),
    "hollow_white": (0.85, 0.85, 0.88),
    "hollow_mask": (0.95, 0.95, 0.97),
    "hollow_void": (0.10, 0.10, 0.12),
    "zanpakuto_steel": (0.70, 0.72, 0.75),
    "zanpakuto_wrap": (0.20, 0.15, 0.10),
    "soul_blue": (0.30, 0.60, 0.95),
    "soul_torch": (0.40, 0.75, 1.00),
    "spirit_pressure": (0.60, 0.30, 0.90),
    "menos_horn": (0.25, 0.25, 0.28),
    "menos_body": (0.50, 0.50, 0.55),
    "soul_orb": (0.50, 0.85, 1.00),
    "fog_veil": (0.70, 0.80, 0.95),
    "deadwood": (0.18, 0.15, 0.12),
    "grave_stone": (0.35, 0.38, 0.42),
    "ruin_stone": (0.30, 0.32, 0.36),
    "bloodstain_green": (0.20, 0.60, 0.30),
}


def height(wx, wz, seed):
    return fbm(wx * FREQ, wz * FREQ, seed) * AMP


def band(tri, seed):
    mx = sum(p[0] for p in tri) / 3
    mz = sum(p[2] for p in tri) / 3
    if fbm(mx * 0.12, mz * 0.12, seed + 555, 3) > 0.72:
        return "spirit_mist"
    return "spirit_drift" if value_noise(mx * 0.4, mz * 0.4, seed + 77) > 0.5 else "spirit_ground"


# --------------------------------------------------- bespoke dressing props

def p_dead_tree(rng):
    def fn(p):
        bark = p("trunk", "deadwood")
        h = 2.0 + rng.uniform(0, 1.2)
        bark.cyl(0, 0, 0, 0.14, 0.07, h)
        for k in range(3):
            bx = (rng.uniform(-1, 1)) * 0.28
            bz = (rng.uniform(-1, 1)) * 0.28
            bh = rng.uniform(0.4, 0.9)
            bark.cyl(bx, h * 0.7, bz, 0.04, 0.0, bh)
    return fn


def p_grave(rng):
    def fn(p):
        st = p("stone", "grave_stone")
        w = 0.30 + rng.uniform(0, 0.15)
        h = 0.50 + rng.uniform(0, 0.40)
        st.box(0, h / 2, 0, w / 2, h / 2, 0.08)
        st.roof_prism(0, h, 0, w / 2, 0.08, 0.12)
    return fn


def p_arch(p):
    st = p("stone", "ruin_stone")
    st.box(-2.6, 1.9, 0, 0.45, 1.9, 0.45)
    st.box(2.6, 1.9, 0, 0.45, 1.9, 0.45)
    st.box(0, 4.0, 0, 3.05, 0.30, 0.50)


def p_pillar(rng):
    def fn(p):
        st = p("stone", "ruin_stone")
        st.cyl(0, 0, 0, 0.35, 0.30, 1.5 + rng.uniform(0, 1.6))
    return fn


def p_fog_gate(p):
    fog = p("veil", "fog_veil")
    fog.box(0, 1.55, 0, 3.1, 1.55, 0.14)


def p_soul_orb(p):
    s = p("soul", "soul_orb")
    s.octahedron(0, 0.25, 0, 0.18)


def p_bloodstain(p):
    b = p("stain", "bloodstain_green")
    b.cyl(0, 0, 0, 0.45, 0.45, 0.02)
    b.cone(0, 0.02, 0, 0.08, 0.20)


def p_soul_torch(p):
    t = p("base", "ruin_stone")
    t.cyl(0, 0, 0, 0.20, 0.15, 0.8)
    flame = p("flame", "soul_torch")
    flame.cone(0, 0.8, 0, 0.15, 0.50)
    glow = p("glow", "soul_blue")
    glow.sphere(0, 1.0, 0, 0.12)


def p_hollow_mask(p):
    """Bespoke hollow mask prop - the iconic BLEACH enemy face."""
    m = p("mask", "hollow_white")
    m.sphere(0, 0, 0, 0.35)
    teeth = p("teeth", "hollow_void")
    for i in range(5):
        angle = -0.4 + i * 0.2
        teeth.box(math.sin(angle) * 0.28, -0.15, math.cos(angle) * 0.25,
                  0.04, 0.06, 0.04)
    eye_l = p("eye_l", "hollow_void")
    eye_l.sphere(-0.12, 0.08, -0.30, 0.06)
    eye_r = p("eye_r", "hollow_void")
    eye_r.sphere(0.12, 0.08, -0.30, 0.06)


def p_menos_grande(p):
    """Bespoke Menos Grande boss - giant hollow with horn and mask."""
    body = p("body", "menos_body")
    body.cyl(0, 0, 0, 0.8, 1.2, 3.5)
    body.cyl(0, 3.0, 0, 0.5, 0.3, 1.5)
    mask = p("mask", "hollow_white")
    mask.sphere(0, 4.2, 0, 0.55)
    horn_l = p("horn_l", "menos_horn")
    horn_l.cone(-0.35, 4.5, 0, 0.08, 0.50)
    horn_r = p("horn_r", "menos_horn")
    horn_r.cone(0.35, 4.5, 0, 0.08, 0.50)
    eye_l = p("eye_l", "spirit_pressure")
    eye_l.sphere(-0.18, 4.3, -0.45, 0.10)
    eye_r = p("eye_r", "spirit_pressure")
    eye_r.sphere(0.18, 4.3, -0.45, 0.10)
    arm_l = p("arm_l", "menos_body")
    arm_l.cyl(-1.0, 2.0, 0, 0.15, 0.10, 1.5)
    arm_r = p("arm_r", "menos_body")
    arm_r.cyl(1.0, 2.0, 0, 0.15, 0.10, 1.5)
    teeth = p("teeth", "hollow_void")
    for i in range(6):
        angle = -0.5 + i * 0.2
        teeth.box(math.sin(angle) * 0.42, 3.8, math.cos(angle) * 0.35,
                  0.05, 0.08, 0.05)


def p_zanpakuto(p):
    """Bespoke Zanpakuto - the Shinigami's soul cutter sword."""
    blade = p("blade", "zanpakuto_steel")
    blade.box(0, 0.0, 0, 0.03, 0.60, 0.01)
    guard = p("guard", "zanpakuto_wrap")
    guard.box(0, -0.60, 0, 0.12, 0.02, 0.04)
    handle = p("handle", "zanpakuto_wrap")
    handle.box(0, -0.90, 0, 0.035, 0.30, 0.035)


# ------------------------------------------------------------------- layout

LANDMARKS = [(0.0, 2.0), (0.0, 35.0), (0.0, 43.0), (0.0, 58.0)]


def layout(rng, reg):
    items = []

    def put(nm, kind, x, z, yaw, tags, w, d):
        pos = reserve_spot(reg, nm, round(float(x), 2), round(float(z), 2), w, d)
        if pos is not None:
            items.append((nm, [pos[0], pos[2]], yaw, list(tags), kind,
                          (w / 2.0, d / 2.0)))
        return pos is not None

    # Fixed story anchors
    for row in (
      ("Soul_Torch_Start", "soul_torch", 0, 2, 0, ["poi", "checkpoint"], 2.0, 2.0),
      ("Player_Start", "soul_torch", 0, 4.5, 0, ["player", "start"], 1.0, 1.0),
      ("Bloodstain", "bloodstain", 2.2, 5, 0, ["memorial", "corpse_run"], 1.2, 1.2),
      ("Ruin_Arch", "arch", 0, 35, 90, ["deco", "gate_frame"], 1.2, 6.2),
      ("Fog_Gate", "fog_gate", 0, 43, 0, ["gate", "boss_entry"], 6.4, 0.6),
      ("Menos_Grande", "menos_grande", 0, 58, 180, ["boss", "aggro_large"], 3.0, 3.0),
    ):
        put(*row)

    # Zanpakuto pickups (soul weapons)
    for i, (lx, lz) in enumerate(((1.8, 3.4), (-2.1, 40.2), (1.6, 54.0)), 1):
        put("Zanpakuto_%02d" % i, "zanpakuto",
            lx + rng.uniform(-0.5, 0.5), lz + rng.uniform(-0.5, 0.5),
            0, ["pickup", "souls"], 0.5, 0.5)

    # Hollow enemies: 4 stalkers spread up the road corridor
    n_hollows = 4 + int(rng.uniform(0, 2))
    for k in range(n_hollows):
        hz = 10 + k * (25.0 / max(1, n_hollows - 1)) + rng.uniform(-2.0, 2.0)
        put("Hollow_%02d" % (k + 1), "stalker", rng.uniform(-4.0, 4.0), hz,
            int(rng.uniform(0, 360)), ["enemy", "aggro_small"], 0.9, 0.9)

    # Hollow mask pickups (purified masks)
    n_masks = 3 + int(rng.uniform(0, 2))
    for k in range(n_masks):
        lx, lz = LANDMARKS[k % len(LANDMARKS)]
        put("Hollow_Mask_%02d" % (k + 1), "hollow_mask",
            lx + rng.uniform(-2.5, 2.5), lz + rng.uniform(-2.5, 2.5),
            0, ["pickup", "souls"], 0.6, 0.6)

    # Soul orbs: 8..10, clustered near landmarks
    n_orbs = 8 + int(rng.uniform(0, 3))
    for k in range(n_orbs):
        lx, lz = LANDMARKS[k % len(LANDMARKS)]
        put("Soul_Orb_%02d" % (k + 1), "soul_orb",
            lx + rng.uniform(-3.0, 3.0), lz + rng.uniform(-3.0, 3.0),
            0, ["pickup", "souls"], 0.5, 0.5)

    # Graves: 8..10, off-corridor
    n_graves = 8 + int(rng.uniform(0, 3))
    for k in range(n_graves):
        gx = rng.pick([-1, 1]) * rng.uniform(3.0, 6.5)
        put("Grave_%02d" % (k + 1), "grave", gx, rng.uniform(5, 40),
            int(rng.uniform(0, 360)), ["deco"], 0.6, 0.6)

    # Dead trees: 5..7 scattered wide
    n_trees = 5 + int(rng.uniform(0, 3))
    for k in range(n_trees):
        tx = rng.pick([-1, 1]) * rng.uniform(2.5, 17.5)
        put("Dead_Tree_%02d" % (k + 1), "dead_tree", tx, rng.uniform(-8, 60),
            int(rng.uniform(0, 360)), ["deco"], 0.9, 0.9)

    # Ruin pillars guard the approach to the arena
    for k in range(4):
        put("Ruin_Pillar_%02d" % (k + 1), "pillar",
            rng.pick([-6.5, -4.5, 4.5, 6.5]), 48 + k * 4, 0, ["deco"], 0.9, 0.9)

    # Banners flanking the fog gate and arena
    for nm, x, z, yaw in (("Banner_Gate_L", -2.0, 42.2, -15),
                          ("Banner_Gate_R", 2.0, 42.2, 15),
                          ("Banner_Arena_L", -3.5, 52.5, -20),
                          ("Banner_Arena_R", 3.5, 52.5, 20)):
        put(nm, "banner", x, z, yaw, ["deco"], 0.5, 0.5)

    return items


# ------------------------------------------------------- mesh kit plumbing

class Kit:
    def __init__(self, mb):
        self.mb = mb

    def __call__(self, pname, mat):
        self.mb.begin(pname, mat)
        return PartHandle(self.mb)


class PartHandle:
    def __init__(self, mb):
        self.mb = mb

    def tri(self, A, B, C):
        self.mb.tri(A, B, C)

    def quad(self, A, B, C, D):
        self.mb.quad(A, B, C, D)

    def box(self, *a):
        self.mb.box(*a)

    def roof_prism(self, *a):
        self.mb.roof_prism(*a)

    def cyl(self, *a, **k):
        self.mb.cyl(*a, **k)

    def cone(self, *a, **k):
        self.mb.cone(*a, **k)

    def octahedron(self, *a):
        self.mb.octahedron(*a)

    def sphere(self, *a, **k):
        self.mb.sphere(*a, **k)


def build(mb, fn):
    fn(Kit(mb))
    return mb


# --------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--radius", type=int, default=2)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    seed_t, seed_s = (666, 2077) if a.seed is None else derive_seeds(a.seed)

    root = Path(a.out_dir)
    models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"

    # --- materials: spirit world palette, then gen_props prop_* MERGE
    write_mtl_for(models, "materials", MATS)
    merged = parse_mtl(models / "materials.mtl")
    pal = PALETTES["haunted_estate"]
    for k, v in pal.items():
        merged.setdefault("prop_" + k, v)
    write_mtl_for(models, "materials", merged)

    made = []
    placed = []
    registry = []

    # --- terrain chunks
    band_fn = lambda tri: band(tri, seed_t)
    for x in range(-a.radius, a.radius + 1):
        for z in range(-a.radius, a.radius + 3):
            cid = "chunk_%d_%d" % (x, z)
            mb = MeshBuilder()
            emit_chunk(mb, "spirit_ground", x, z, CHUNK, RES, seed_t, height, band_fn)
            obj_text, nv, nf = mb.to_obj(cid, "materials")
            p = models / (cid + ".obj")
            if not p.exists():
                p.write_text(obj_text, encoding="utf-8")
                made.append(cid + ".obj")
            registry.append((cid, "models/" + cid + ".obj"))
            placed.append((cid, [0, 0, 0], 0, ["terrain"]))
    for cid, rel in registry:
        register_index(assets_dir, cid, rel)

    # --- props: souls kit from gen_props.build_prop, then bespoke dressing
    ppal = {"prop_" + k: v for k, v in pal.items()}
    rng = Rng(seed_s)

    for name in ("bonfire", "stalker", "knight", "estus_flask", "banner"):
        save_prop(models, name, build_prop(name, ppal), "materials", merged,
                  assets_dir=assets_dir, auto_recenter=True)
        made.append(name + ".obj")

    for name, fn, centered in [("bloodstain", p_bloodstain, False),
                               ("grave", p_grave(rng), False),
                               ("dead_tree", p_dead_tree(rng), False),
                               ("pillar", p_pillar(rng), False),
                               ("arch", p_arch, False),
                               ("fog_gate", p_fog_gate, False),
                               ("soul_orb", p_soul_orb, False),
                               ("soul_torch", p_soul_torch, False),
                               ("hollow_mask", p_hollow_mask, False),
                               ("menos_grande", p_menos_grande, False),
                               ("zanpakuto", p_zanpakuto, False)]:
        save_prop(models, name, build(MeshBuilder(), fn), "materials", merged,
                  assets_dir=assets_dir,
                  enforce_origin=centered, auto_recenter=not centered)
        made.append(name + ".obj")

    # --- scene nodes
    reg = Placement()
    reg.register_height_field(
        "spirit_terrain", lambda wx, wz: height(wx, wz, seed_t),
        ((-a.radius * CHUNK, -a.radius * CHUNK),
         ((a.radius + 1) * CHUNK, (a.radius + 3) * CHUNK)))
    for nm, xz, yaw, tags, kind, fp in layout(rng, reg):
        h = reg.query_height(xz[0], xz[1])
        y = round(h, 3) if h is not None else 0.0
        placed.append((nm, [xz[0], y, xz[1]], yaw, tags, kind, fp))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed,
                "bleach-soul-reaper", placement=reg)

    # --- world state LAST, then log
    state = {
        "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
        "theme": "bleach-soul-reaper",
        "identity": {"movement": "soulslike third-person stamina sprint",
                     "camera": "third-person orbit"},
        "updated": datetime.datetime.now().isoformat(timespec="seconds"),
        "seed": {"input": a.seed, "terrain": seed_t, "scatter": seed_s},
        "chunk_size": CHUNK, "radius": a.radius,
        "camera": {"target": [0, 1.5, 28], "distance": 28},
        "chunks": [{"id": c, "path": "assets/" + r, "position": [0, 0, 0]}
                   for c, r in registry],
        "palette": MATS,
        "gameplay": {
            "genre": "soulslike",
            "objective": "light the Soul Torch, fight through the Spirit World, cross the fog gate and slay the Menos Grande",
            "corpse_run": True,
            "physics": {"gravity": -22.0, "jump_velocity": 8.0, "run_speed": 6.5,
                        "coyote_time_s": 0.10, "jump_buffer_s": 0.12},
            "enemy_aggro_m": 8.0, "kill_radius_m": 2.2, "interact_radius_m": 2.4,
            "lives": 0, "score_goal": 1500,
            "scoring": {"per_orb": 25, "checkpoint_light": 150, "boss_kill": 1000,
                        "zanpakuto": 200, "hollow_mask": 75},
            "hazards": ["Hollow ambushes along the spirit road",
                        "The Menos Grande beyond the fog gate"],
            "checkpoints": ["Soul_Torch_Start"]}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "BLEACH: SOUL REAPER soulslike world (seed %s -> terrain %d, scatter %d)"
               % (a.seed, seed_t, seed_s),
               ["%d terrain chunks (%.0fm grid, res %d, road-extended +Z)" % (len(registry), CHUNK, RES),
                "%d prop models (souls kit + BLEACH bespoke), %d scene nodes; "
                "soul torch/corpse-run/fog-gate/boss contract in world_state.json"
                % (len(made) - len(registry), len(placed) - len(registry))])
    print("[bleach] ready: %d chunks + %d assets | %d scene nodes | seed %s (T%d/S%d)"
          % (len(registry), len(made), len(placed), a.seed, seed_t, seed_s))


if __name__ == "__main__":
    main()
