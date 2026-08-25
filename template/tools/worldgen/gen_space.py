#!/usr/bin/env python3
"""VOID DRIFT - space salvage world generator (Litt Engine).

Derelict station, asteroid clumps, escape pods, star canopy over a void plane.
Genre math: dart-thrown deterministic scatter (cookbook sec 6), Rng xorshift32.
Instancing contract (audit item 15): ONE star mesh + N instance nodes,
AST_VARIANTS asteroid variant meshes + instance nodes, ONE pod mesh + N pod
nodes - OBJ count stays flat (~8) regardless of star count. Meshes are modeled
at origin; every placement rides the node position only.

Collision-safe scatter (audit item 16): every solid XZ footprint (station
exclusion ring r 3.4 + margin, asteroids, pods, jump gate) registers in a
worldkit.Placement registry; overlapping candidates are re-rolled a bounded
number of times then skipped - same seed always yields the same accept/reject
sequence because the registry iterates in insertion order.

Goal semantics (audit item 16): exactly ONE goal-tagged node (Jump_Gate at the
+X map edge, origin-centered shared mesh), >=5 pickup-tagged escape pods, and
the Derelict_Station keeps its hub tag. state.gameplay carries a structured
physics dict (no prose movement) plus hazard node names; hazards themselves
are the hazard-tagged asteroid nodes.
Usage: python gen_space.py [--out-dir .] [--agent ai] [--prompt "..."]
"""
import argparse
import datetime
from pathlib import Path

from worldkit import (Rng, MeshBuilder, write_mtl_for, register_index,
                      write_scene, write_state, append_log, save_prop,
                      Placement)

SEED_S = 4242
AST_VARIANTS = 4   # shared asteroid meshes; instances pick one per node
SOLID_HALF = 2.0       # uniform XZ half-extent of every registered solid (m);
                       # AABB rejection then guarantees pairwise center
                       # distance >= 4.0 m (= sum of approx 2 m radii)
STATION_HALF = 4.6     # station exclusion: ring r~3.4 + margin
GATE_X = 46.0          # jump gate just outside the +-40 asteroid belt
PLACE_ATTEMPTS = 12    # bounded re-rolls per candidate, then skip
MATS = {
  "void_plane": (0.04, 0.055, 0.10), "star_white": (0.95, 0.96, 1.0),
  "star_blue": (0.55, 0.70, 1.0), "star_gold": (1.0, 0.85, 0.45),
  "rock_grey": (0.36, 0.35, 0.34), "rock_brown": (0.42, 0.33, 0.26),
  "hull_steel": (0.30, 0.33, 0.38), "hull_oxide": (0.45, 0.28, 0.18),
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
    greeble = p("greeble", "hull_oxide")
    greeble.box(-1.4, 5.6, 0.9, 0.8, 0.5, 0.8)
    greeble.box(1.6, 2.6, -1.0, 0.6, 0.4, 0.6)
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

def p_gate(p):
    """Jump gate, modeled AT ORIGIN (x/z symmetric): two pylons + beacon bar."""
    pyl = p("pylons", "hull_steel")
    pyl.cyl(-1.3, 0, 0, 0.10, 0.16, 2.6, seg=8)
    pyl.cyl( 1.3, 0, 0, 0.10, 0.16, 2.6, seg=8)
    bcn = p("beacon", "pod_orange"); bcn.box(0, 2.72, 0, 2.9, 0.16, 0.16)

def build(mb, fn):
    fn(Kit(mb))
    return mb

def try_place(reg, name, cx, cz, half):
    """Reserve one centered XZ footprint; False (no mutation) on conflict."""
    x, z = round(cx, 2), round(cz, 2)
    mn, mx = (x - half, z - half), (x + half, z + half)
    if reg.conflicts(mn, mx):
        return False
    return reg.insert(name, mn, mx)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--asteroids", type=int, default=24)
    ap.add_argument("--stars", type=int, default=280)
    ap.add_argument("--pods", type=int, default=6)
    ap.add_argument("--seed", type=int, default=SEED_S)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)
    rng = Rng(a.seed)
    reg = Placement()          # audit item 16: collision-safe scatter
    made = []; placed = []

    # AUDIT 3.3 fix - CHOSEN: lift the void plane so its TOP rests at y=0
    # (cylinder y0=-0.2, h=0.2; was y0=-2 -> top at -1.8 with pods/gate
    # hovering 1.8 m above it). Snapping pods+gate down to -1.8 instead was
    # rejected: y=0 play plane matches every other generator and keeps pod/
    # gate/station node data untouched. Documented in state.gameplay.note.
    mb = build(MeshBuilder(), lambda p: p("void", "void_plane").cyl(0, -0.2, 0, 200, 200, 0.2, seg=24))
    save_prop(models, "void_plane", mb, "materials", MATS, assets_dir)
    made.append("void_plane.obj")
    placed.append(("Void_Plane", [0, 0, 0], 0, ["backdrop", "terrain", "floor"]))

    # ONE star mesh, instanced N times (hub_spoke coin pattern): the shared
    # glint octahedron carries an explicit model: tag so every node resolves
    # to the single star.obj instead of one OBJ per star. Backdrop only -
    # stars fly at y>=14 above the play plane and never enter the registry.
    mb = build(MeshBuilder(), p_star)
    save_prop(models, "star", mb, "materials", MATS, assets_dir,
              enforce_origin=True)
    made.append("star.obj")
    for i in range(a.stars):
        sx, sy, sz = round(rng.uniform(-90,90),2), round(rng.uniform(14,60),2), round(rng.uniform(-90,90),2)
        placed.append(("Star_%03d" % i, [sx, sy, sz], 0, ["backdrop","star","model:star"]))

    # Station hub first: its whole exclusion footprint (ring r~3.4 + margin)
    # blocks the registry so nothing scatters through the derelict.
    mb = build(MeshBuilder(), p_station)
    save_prop(models, "derelict_station", mb, "materials", MATS, assets_dir,
              auto_recenter=True)
    made.append("derelict_station.obj")
    reg.insert("Derelict_Station", (-STATION_HALF, -STATION_HALF),
               ( STATION_HALF,  STATION_HALF))
    placed.append(("Derelict_Station", [0, 0, 0], 0,
                   ["poi", "salvage", "level", "hub"], "derelict_station",
                   (STATION_HALF, STATION_HALF)))

    # Exactly ONE goal node: jump gate at the +X map edge, origin-centered
    # shared mesh, registered like any other solid so pods keep clear of it.
    mb = build(MeshBuilder(), p_gate)
    save_prop(models, "jump_gate", mb, "materials", MATS, assets_dir,
              enforce_origin=True)
    made.append("jump_gate.obj")
    if not try_place(reg, "Jump_Gate", GATE_X, 0.0, SOLID_HALF):
        raise SystemExit("jump gate footprint blocked - cannot ship without "
                         "its goal node")
    placed.append(("Jump_Gate", [GATE_X, 0, 0], 90,
                   ["goal", "poi", "jump_gate"], "jump_gate",
                   (SOLID_HALF, SOLID_HALF)))

    # A few seeded asteroid VARIANT meshes, then one hazard node per asteroid.
    # Each candidate is re-rolled against the registry (bounded retries, then
    # skipped) so rocks never overlap each other or the reserved footprints.
    variant_names = []
    for v in range(1, AST_VARIANTS + 1):
        name = "asteroid_v%02d" % v
        mb = build(MeshBuilder(), p_asteroid(rng))
        save_prop(models, name, mb, "materials", MATS, assets_dir,
                  auto_recenter=True)
        made.append(name + ".obj")
        variant_names.append(name)
    ast_names = []; skipped_ast = 0
    for i in range(a.asteroids):
        ref = variant_names[i % len(variant_names)]
        nm = "Asteroid_%02d" % (i+1)
        pos = None
        for _ in range(PLACE_ATTEMPTS):
            ax = round(rng.uniform(-40,40),2); ay = round(rng.uniform(0,8),2)
            az = round(rng.uniform(-40,40),2); yaw = int(rng.uniform(0,360))
            if try_place(reg, nm, ax, az, SOLID_HALF):
                pos = [ax, ay, az]; break
        if pos is None:
            skipped_ast += 1
            continue
        ast_names.append(nm)
        placed.append((nm, pos, yaw, ["hazard", "model:" + ref],
                       ref, (SOLID_HALF, SOLID_HALF)))

    # Escape pods share ONE mesh; retagged pickup (audit item 16) so the
    # salvage loop is machine-readable: pickup pods + one goal gate + hub.
    mb = build(MeshBuilder(), p_pod)
    save_prop(models, "escape_pod", mb, "materials", MATS, assets_dir,
              auto_recenter=True)
    made.append("escape_pod.obj")
    pod_names = []; skipped_pods = 0
    for i in range(a.pods):
        nm = "Escape_Pod_%02d" % (i+1)
        pos = None
        for _ in range(PLACE_ATTEMPTS):
            px = round(rng.uniform(-25,25),2); pz = round(rng.uniform(-25,25),2)
            yaw = int(rng.uniform(0,360))
            if try_place(reg, nm, px, pz, SOLID_HALF):
                pos = [px, 0, pz]; break
        if pos is None:
            skipped_pods += 1
            continue
        pod_names.append(nm)
        placed.append((nm, pos, yaw,
                       ["pickup", "salvage", "model:escape_pod"],
                       "escape_pod", (SOLID_HALF, SOLID_HALF)))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed,
                "void-drift", placement=reg)
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
                   "objective": "recover all escape pods, then exit through "
                                "Jump_Gate at the map edge",
                   "note": "audit 3.3: void-plane TOP lifted to y=0 "
                           "(cyl y0=-0.2 h=0.2); pods and Jump_Gate rest on "
                           "the plane with zero node-y offset",
                   "goals": ["Jump_Gate"],
                   "pickups": pod_names,
                   "hub": ["Derelict_Station"],
                   "physics": {"movement": "6dof_thrusters", "gravity": 0.0,
                               "run_speed": 18.0, "thrust_accel_m_s2": 14.0,
                               "linear_damping": 0.40, "max_speed_m_s": 26.0,
                               "turn_rate_deg_s": 90.0},
                   "hazards": {"kind": "asteroid_field",
                               "nodes": ast_names,
                               "hull_nodes": ["Derelict_Station"]},
                   "scoring": {"per_pod": 100, "hull_hit_penalty": 15,
                               "gate_jump": 500}}
    }
    write_state(root / "world_state.json", state)

    census = lambda tag: sum(1 for it in placed if tag in it[3])
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "VOID DRIFT space-salvage world (seed %d)" % a.seed,
               ["%d stars on 1 instanced mesh, %d/%d asteroids on %d variant "
                "meshes, %d/%d pods on 1 mesh; station + void plane"
                % (a.stars, len(ast_names), a.asteroids, AST_VARIANTS,
                   len(pod_names), a.pods),
                "%d scene nodes from only %d OBJ files (instancing); "
                "Placement registry kept every solid footprint overlap-free "
                "(skipped: %d asteroids, %d pods)"
                % (len(placed), len(made), skipped_ast, skipped_pods),
                "goal semantics: 1 Jump_Gate (goal) at +X edge, %d pickup "
                "pods, Derelict_Station hub; structured physics dict in "
                "state.gameplay" % len(pod_names)])
    print("[voiddrift] ready: %d assets, %d scene nodes | census: goal=%d "
          "pickup=%d hub=%d hazard=%d | skipped: ast=%d pods=%d"
          % (len(made), len(placed), census("goal"), census("pickup"),
             census("hub"), census("hazard"), skipped_ast, skipped_pods))

if __name__ == "__main__":
    main()
