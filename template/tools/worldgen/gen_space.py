#!/usr/bin/env python3
"""VOID DRIFT - space salvage world generator (Litt Engine).

Derelict station, asteroid clumps, escape pods, star canopy over a void plane.
Genre math: dart-thrown deterministic scatter (cookbook sec 6), Rng xorshift32.
Instancing contract (audit item 15): ONE star mesh + N instance nodes,
AST_VARIANTS asteroid variant meshes + instance nodes, ONE pod mesh + N pod
nodes - OBJ count stays flat (~8) regardless of star count. Meshes are modeled
at origin; every placement rides the node position only.
Usage: python gen_space.py [--out-dir .] [--agent ai] [--prompt "..."]
"""
import argparse
import datetime
from pathlib import Path

from worldkit import (Rng, MeshBuilder, write_mtl_for, register_index,
                      write_scene, write_state, append_log, save_prop)

SEED_S = 4242
AST_VARIANTS = 4   # shared asteroid meshes; instances pick one per node
MATS = {
  "void_plane": (0.04, 0.055, 0.10), "star_white": (0.95, 0.96, 1.0),
  "star_blue": (0.55, 0.70, 1.0), "star_gold": (1.0, 0.85, 0.45),
  "rock_grey": (0.36, 0.35, 0.34), "rock_brown": (0.42, 0.33, 0.26),
  "hull_steel": (0.30, 0.33, 0.38), "hull_rust": (0.45, 0.28, 0.18),
  "pod_orange": (0.90, 0.50, 0.12), "antenna_grey": (0.55, 0.57, 0.60),
}

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
    def pyramid(self, *a): self.mb.pyramid(*a)
    def cyl(self, *a, **k): self.mb.cyl(*a, **k)
    def cone(self, *a, **k): self.mb.cone(*a, **k)
    def octahedron(self, *a): self.mb.octahedron(*a)

def p_station(p):
    hull = p("core", "hull_steel");   hull.box(0, 4, 0, 2.2, 1.4, 2.2)
    rust = p("greeble", "hull_rust")
    rust.box(-1.4, 5.6, 0.9, 0.8, 0.5, 0.8)
    rust.box(1.6, 2.6, -1.0, 0.6, 0.4, 0.6)
    ring = p("ring", "antenna_grey"); ring.cyl(0, 3.85, 0, 3.4, 3.4, 0.30, seg=14)
    ant  = p("mast", "antenna_grey"); ant.cone(0, 5.4, 0, 0.10, 2.6, seg=6)

def p_asteroid(rng):
    def fn(p):
        m = p("rock", "rock_grey" if rng.uniform() > 0.5 else "rock_brown")
        w = rng.uniform(0.5, 1.6)
        m.box(0, w*0.6, 0, w/2, w*0.45, w*0.4)
        m.box(w*0.3, w*0.9, w*0.2, w*0.28, w*0.25, w*0.25)
    return fn

def p_pod(p):
    shell = p("shell", "pod_orange"); shell.box(0, 0.5, 0, 0.45, 0.5, 0.65)
    port  = p("port", "star_blue");   port.box(0, 0.72, 0.5, 0.22, 0.16, 0.05)

def p_star(p):
    s = p("glint", "star_white"); s.octahedron(0, 0.12, 0, 0.14)

def build(mb, fn):
    fn(Kit(mb))
    return mb

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--asteroids", type=int, default=24)
    ap.add_argument("--stars", type=int, default=280)
    ap.add_argument("--pods", type=int, default=4)
    ap.add_argument("--seed", type=int, default=SEED_S)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)
    rng = Rng(a.seed)
    made = []; placed = []

    mb = build(MeshBuilder(), lambda p: p("void", "void_plane").cyl(0, -2, 0, 200, 200, 0.2, seg=24))
    save_prop(models, "void_plane", mb, "materials", MATS, assets_dir)
    made.append("void_plane.obj")
    placed.append(("Void_Plane", [0, 0, 0], 0, ["backdrop", "terrain", "floor"]))

    # ONE star mesh, instanced N times (hub_spoke coin pattern): the shared
    # glint octahedron carries an explicit model: tag so every node resolves
    # to the single star.obj instead of one OBJ per star.
    mb = build(MeshBuilder(), p_star)
    save_prop(models, "star", mb, "materials", MATS, assets_dir)
    made.append("star.obj")
    for i in range(a.stars):
        sx, sy, sz = round(rng.uniform(-90,90),2), round(rng.uniform(14,60),2), round(rng.uniform(-90,90),2)
        placed.append(("Star_%03d" % i, [sx, sy, sz], 0, ["backdrop","star","model:star"]))

    mb = build(MeshBuilder(), p_station)
    save_prop(models, "derelict_station", mb, "materials", MATS, assets_dir)
    made.append("derelict_station.obj")
    placed.append(("Derelict_Station", [0, 0, 0], 0, ["poi", "salvage", "level", "hub"]))

    # A few seeded asteroid VARIANT meshes, then one hazard node per asteroid.
    variant_names = []
    for v in range(1, AST_VARIANTS + 1):
        name = "asteroid_v%02d" % v
        mb = build(MeshBuilder(), p_asteroid(rng))
        save_prop(models, name, mb, "materials", MATS, assets_dir)
        made.append(name + ".obj")
        variant_names.append(name)
    for i in range(a.asteroids):
        ref = variant_names[i % len(variant_names)]
        ax, ay, az = round(rng.uniform(-40,40),2), round(rng.uniform(0,8),2), round(rng.uniform(-40,40),2)
        placed.append(("Asteroid_%02d" % (i+1),
                       [ax, ay, az], int(rng.uniform(0,360)),
                       ["hazard", "model:" + ref]))

    # Escape pods share ONE mesh; gameplay tags stay exactly as before.
    mb = build(MeshBuilder(), p_pod)
    save_prop(models, "escape_pod", mb, "materials", MATS, assets_dir)
    made.append("escape_pod.obj")
    for i in range(a.pods):
        px, pz = round(rng.uniform(-25,25),2), round(rng.uniform(-25,25),2)
        placed.append(("Escape_Pod_%02d" % (i+1), [px, 0, pz],
                       int(rng.uniform(0,360)), ["objective","salvage","model:escape_pod"]))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "void-drift")
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "void-drift",
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"scatter": a.seed},
      "chunk_size": 0, "radius": 0,
      "camera": {"target": [0, 3, 0], "distance": 32},
      "chunks": [],
      "palette": MATS,
      "gameplay": {"genre": "space_salvage",
                   "movement": "6DOF thrusters, mild damping",
                   "objective": "dock the escape pods before drift carries them off",
                   "hazards": ["asteroid collisions", "derelict hull edges"],
                   "scoring": {"per_pod": 100, "hull_hit_penalty": 15}}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "VOID DRIFT space-salvage world (seed %d)" % a.seed,
               ["%d stars on 1 instanced mesh, %d asteroids on %d variant meshes, "
                "%d pods on 1 mesh; station + void plane" %
                (a.stars, a.asteroids, AST_VARIANTS, a.pods),
                "%d scene nodes placed from only %d OBJ files (instancing)" %
                (len(placed), len(made))])
    print("[voiddrift] ready: %d assets, %d scene nodes" % (len(made), len(placed)))

if __name__ == "__main__":
    main()
