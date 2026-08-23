#!/usr/bin/env python3
"""RUSTY CINDER RUN - 2.5D platformer level generator (Litt Engine).

A side-scroller corridor along +X: floor runs with death pits, floating
platforms, spike hazards, coin arcs, parallax backdrop slabs, goal flag.
Genre math: jump-arc feasibility (v^2 = 2*g*h, range = v_x * 2*v_y/g)
drives gap widths; parallax depth layering for the backdrop.
Usage: python gen_platformer25d.py [--out-dir .] [--agent ai] [--prompt "..."]
"""
import argparse
import datetime
from pathlib import Path

from worldkit import (MeshBuilder, write_mtl_for, register_index,
                      write_scene, write_state, append_log, save_prop)

SEED_S = 909
MATS = {
  "cinder_floor": (0.38, 0.26, 0.20), "cinder_dark": (0.24, 0.17, 0.14),
  "steel_platform": (0.45, 0.47, 0.50), "spike_iron": (0.60, 0.60, 0.63),
  "coin_amber": (0.95, 0.72, 0.15), "flag_red": (0.80, 0.18, 0.15),
  "pole_grey": (0.40, 0.42, 0.44),
  "bg_far": (0.16, 0.12, 0.11), "bg_near": (0.28, 0.19, 0.15),
}

class Kit:
    def __init__(self, mb): self.mb = mb
    def __call__(self, pname, mat):
        self.mb.begin(pname, mat)
        return PartHandle(self.mb)

class PartHandle:
    def __init__(self, mb): self.mb = mb
    def box(self, *a): self.mb.box(*a)
    def cyl(self, *a, **k): self.mb.cyl(*a, **k)
    def cone(self, *a, **k): self.mb.cone(*a, **k)
    def prism(self, *a): self.mb.roof_prism(*a)
    def octahedron(self, *a): self.mb.octahedron(*a)

# Jump math: g=30, v_y=12 -> max height h=v^2/2g=2.4 m; airtime t=2*v/g=0.8 s;
# with run speed 8 m/s -> max clearable gap ~= 6.4 m. Gaps stay at 3.0-4.5 m.
GAPS = [(14.0, 3.2), (30.0, 4.2), (46.5, 3.8), (62.0, 4.5)]
LEVEL_LEN = 78.0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--seed", type=int, default=SEED_S)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)
    made = []; placed = []

    mb = MeshBuilder(); kit = Kit(mb)
    floor = kit("floor", "cinder_floor")
    edges = [(-6.0)] + [g[0] for g in GAPS] + [LEVEL_LEN]
    seg_starts = [-6.0]
    for gx, gw in GAPS:
        seg_starts.append(gx + gw)
    pairs = []
    starts = [-6.0] + [g[0] + g[1] for g in GAPS]
    ends   = [g[0] for g in GAPS] + [LEVEL_LEN]
    for s0, s1 in zip(starts, ends):
        if s1 > s0:
            cxm = (s0 + s1) / 2.0; w = s1 - s0
            floor.box(cxm, -0.55, 0, w/2, 0.55, 1.3)
    dark = kit("pit_floor", "cinder_dark")
    for gx, gw in GAPS:
        dark.box(gx + gw/2, -2.0, 0, gw/2, 0.2, 1.3)
    spikes = kit("spikes", "spike_iron")
    for gx, gw in GAPS:
        n = int(gw / 0.7)
        for k in range(n):
            sxp = gx + 0.35 + k * 0.7
            spikes.cone(sxp, -1.8, 0, 0.16, 0.5, seg=6)
    name = "level_track"
    obj_text, nv, nf = mb.to_obj(name, "materials")
    (models / (name + ".obj")).write_text(obj_text, encoding="utf-8")
    register_index(assets_dir, name, "models/" + name + ".obj")
    made.append(name + ".obj")
    placed.append(("Level_Track", [LEVEL_LEN/2 - 3, 0, 0], 0, ["level","floor"]))

    plats = [(10.5, 1.6, 2.6), (27.0, 2.4, 2.2), (33.5, 1.2, 2.8), (43.0, 2.8, 2.4),
             (58.5, 1.9, 2.6), (66.0, 3.1, 2.2), (71.0, 2.2, 2.6)]
    for i, (px, py, pw) in enumerate(plats):
        nm = "Platform_%02d" % (i+1)
        mb = MeshBuilder(); k = Kit(mb); pf = k("deck", "steel_platform")
        pf.box(px, py, 0, pw/2, 0.12, 1.1)
        save_prop(models, nm, mb, "materials", MATS, assets_dir)
        made.append(nm + ".obj")
        placed.append((nm, [px, py, 0], 0, ["platform"]))

    coin_id = 0
    for gx, gw in GAPS:
        for arc in (-0.5, 0.0, 0.5):
            coin_id += 1
            nm = "Coin_%02d" % coin_id
            mb = MeshBuilder(); k = Kit(mb); co = k("coin", "coin_amber")
            co.octahedron(gx + gw/2 + arc*gw*0.5, 1.5 + (0.4 - abs(arc)*0.8) + 0.6, 0, 0.16)
            save_prop(models, nm, mb, "materials", MATS, assets_dir)
            made.append(nm + ".obj")
            placed.append((nm, [round(gx + gw/2 + arc*gw*0.5, 2), round(2.1 - abs(arc)*0.8 + 0.4, 2), 0], 0, ["pickup","score"]))

    mb = MeshBuilder(); k = Kit(mb); fl = k("pole", "pole_grey")
    fl.cyl(LEVEL_LEN - 2, 0, 0, 0.06, 0.05, 3.2, seg=8)
    fg = k("cloth", "flag_red"); fg.prism(LEVEL_LEN - 2, 2.5, 0.45, 0.05, 0.45, 0.55)
    save_prop(models, "goal_flag", mb, "materials", MATS, assets_dir)
    made.append("goal_flag.obj")
    placed.append(("Goal_Flag", [LEVEL_LEN - 2, 0, 0], 0, ["goal","win"]))

    for depth, (zoff, h, matname) in enumerate([(-5.5, 7.0, "bg_near"), (-11.0, 11.0, "bg_far")]):
        nm = "Backdrop_%s" % ("Near" if depth == 0 else "Far")
        mb = MeshBuilder(); k = Kit(mb); bg = k("slab", matname)
        bg.box(LEVEL_LEN/2 - 3, h/2 - 1.0, zoff, LEVEL_LEN/2 + 8, h/2, 0.3)
        save_prop(models, nm, mb, "materials", MATS, assets_dir)
        made.append(nm + ".obj")
        placed.append((nm, [LEVEL_LEN/2 - 3, 0, zoff], 0, ["backdrop","parallax_" + str(depth+1)]))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "rusty-cinder-run")
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "rusty-cinder-run",
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"layout": a.seed},
      "chunk_size": 0, "radius": 0,
      "camera": {"target": [30, 1.5, 0], "distance": 26},
      "chunks": [],
      "palette": MATS,
      "gameplay": {"genre": "platformer_2_5d",
                   "physics": {"gravity": 30, "jump_velocity": 12, "run_speed": 8,
                               "max_jump_height_m": 2.4, "max_gap_m": 6.4,
                               "coyote_time_s": 0.10, "jump_buffer_s": 0.12},
                   "hazards": {"spikes": "instant respawn at level start", "pits": "fall = respawn"},
                   "scoring": {"coins": 12, "goal_bonus": 250}}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "RUSTY CINDER RUN 2.5D level (seed %d)" % a.seed,
               ["%d m track, %d gaps with spike pits, %d platforms, %d coins, goal flag" % (LEVEL_LEN, len(GAPS), len(plats), coin_id),
                "jump-arc verified gaps (<= 6.4 m max range); physics constants in state"])
    print("[platformer] ready: %d assets, %d scene nodes" % (len(made), len(placed)))

if __name__ == "__main__":
    main()