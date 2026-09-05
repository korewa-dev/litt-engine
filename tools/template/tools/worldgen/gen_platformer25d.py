#!/usr/bin/env python3
"""RUSTY CINDER RUN - 2.5D platformer level generator (Litt Engine).

A side-scroller corridor along +X: floor runs with death pits, floating
platforms, spike hazards, coin arcs, parallax backdrop slabs, goal flag.
Genre math: jump-arc feasibility (algokit.solve_jump_arc: h=v^2/2g,
range=v_x*2v/g) caps every generated gap width at physically clearable
ranges; --seed drives ALL layout numbers through worldkit.Rng so the same
seed reproduces the level byte-for-byte (WORLDGEN_AUDIT item 19).

Conventions (WORLDGEN_AUDIT items 20/21):
  * every mesh is built AT ORIGIN and placed purely via node.position -
    coins share ONE coin.obj through instance nodes, platforms come in
    three reusable widths instanced at seed-driven spots; the walkable
    track stays a single level-node mesh whose vertices are relative to
    that node's pivot (never absolute world coords);
  * pit spans and spike clusters are dedicated origin-centered hazard
    meshes placed as nodes tagged ["hazard"] - native/littcore/litt_world.c
    maps the "hazard" tag to LV_F_HAZARD kill/respawn semantics, so no
    fake pickup/enemy tags are emitted;
  * state.gameplay.hazards prose documents those same nodes.

Usage: python gen_platformer25d.py [--out-dir .] [--seed 909]
                                   [--agent ai-agent] [--prompt "..."]
"""
import argparse
import datetime
from pathlib import Path

from algokit import can_clear_gap, solve_jump_arc
from worldkit import (MeshBuilder, Rng, append_log, save_prop,
                      write_mtl_for, write_scene, write_state)

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

# Jump math (mirrored verbatim into state.gameplay.physics):
# g=30 m/s^2, v_y=12 m/s -> peak height v^2/2g = 2.4 m, airtime 2v/g = 0.8 s;
# run speed 8 m/s -> max clearable gap = run*airtime = 6.4 m. Generated gaps
# stay within [GAP_MIN_M, GAP_MAX_M] and are re-checked with algokit's
# can_clear_gap so nothing unjumpable can ever ship.
GRAVITY, JUMP_V, RUN_SPEED = 30, 12, 8
GAP_MIN_M, GAP_MAX_M = 3.0, 4.6
# Three reusable deck meshes (half-widths -> 2.0 / 2.6 / 3.2 m decks).
PLATFORM_VARIANTS = (("platform_short", 1.0),
                     ("platform_mid", 1.3),
                     ("platform_long", 1.6))

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

    arc = solve_jump_arc(JUMP_V, GRAVITY, RUN_SPEED)
    max_gap_m = min(arc["max_range_m"], 6.4)
    assert GAP_MAX_M <= max_gap_m, "generated gap range exceeds jump physics"

    # ---- TASK 19: every layout number comes from Rng(seed) -------------
    rng = Rng(a.seed)
    n_gaps = 4 + rng.next_u32() % 2                    # 4..5 pits
    gaps, xcur = [], 12.0
    for _ in range(n_gaps):
        gx = round(xcur + rng.uniform(11.0, 16.0), 2)  # run before the pit
        gw = round(rng.uniform(GAP_MIN_M, GAP_MAX_M), 2)
        assert can_clear_gap(gw, JUMP_V, GRAVITY, RUN_SPEED), \
            "gap %.2f m exceeds clearable %.2f m" % (gw, max_gap_m)
        gaps.append((gx, gw))
        xcur = gx + gw
    level_len = round(xcur + 14.0, 2)                  # run-out + flag room
    track_x = round(level_len / 2.0, 2)                # track node pivot

    n_plats = 5 + rng.next_u32() % 3                   # 5..7 platforms
    plats, tries = [], 0
    while len(plats) < n_plats and tries < 200:
        tries += 1
        px = round(rng.uniform(8.0, level_len - 12.0), 2)
        py = round(rng.uniform(1.2, 2.2), 2)           # <= peak jump height
        if any(abs(px - q[0]) < 4.0 for q in plats):
            continue                                   # keep decks apart
        plats.append((px, py))

    made, placed = [], []

    def prop(name, mb, enforce_origin=True):
        """TASK 20 convention: write one AT-ORIGIN mesh; placement lives
        solely in the scene node position tuples appended by callers."""
        save_prop(models, name, mb, "materials", MATS, assets_dir=assets_dir,
                  enforce_origin=enforce_origin)
        made.append(name + ".obj")

    # ---- walkable track: ONE level-node mesh, verts relative to pivot --
    mb = MeshBuilder(); kit = Kit(mb); floor = kit("floor", "cinder_floor")
    starts = [-6.0] + [g[0] + g[1] for g in gaps]
    ends = [g[0] for g in gaps] + [level_len]
    for s0, s1 in zip(starts, ends):
        if s1 > s0:
            cxm = (s0 + s1) / 2.0 - track_x
            floor.box(cxm, -0.55, 0, (s1 - s0) / 2, 0.55, 1.3)
    prop("level_track", mb, enforce_origin=False)  # sanctioned level-node
    placed.append(("Level_Track", [track_x, 0, 0], 0, ["level", "floor"],
                   "level_track"))

    # ---- platforms: three reusable decks instanced per slot ------------
    for vname, hw in PLATFORM_VARIANTS:
        vmb = MeshBuilder(); vk = Kit(vmb); dk = vk("deck", "steel_platform")
        dk.box(0, 0, 0, hw, 0.12, 1.1)                 # centered at origin
        prop(vname, vmb)
    for i, (px, py) in enumerate(plats):
        vi = rng.next_u32() % len(PLATFORM_VARIANTS)
        placed.append(("Platform_%02d" % (i + 1), [px, py, 0], 0,
                       ["platform"], PLATFORM_VARIANTS[vi][0]))

    # ---- coins: ONE mesh, every coin an instance node ------------------
    cmb = MeshBuilder(); ck = Kit(cmb); co = ck("coin", "coin_amber")
    co.octahedron(0, 0, 0, 0.16)
    prop("coin", cmb)
    n_coins = 0
    for gx, gw in gaps:
        offs = sorted(rng.uniform(-0.5, 0.5) for _ in range(3))
        for off in offs:
            n_coins += 1
            cxp = round(gx + gw / 2 + off * gw * 0.5, 2)
            cyp = round(1.5 + 0.8 * (1.0 - abs(off))
                        + rng.uniform(-0.1, 0.1), 2)
            placed.append(("Coin_%02d" % n_coins, [cxp, cyp, 0], 0,
                           ["pickup", "score"], "coin"))

    # ---- TASK 21: hazards leave the baked track, become tagged nodes ---
    # Node sits mid-gap at y=-1.5 so the engine's interact-radius kill
    # sphere triggers inside the pit yet never clips legitimate jumps;
    # meshes below bake only the offsets relative to that node origin.
    pmb = MeshBuilder(); pk = Kit(pmb); pz = pk("pit", "cinder_dark")
    pz.box(0, -0.5, 0, 1.4, 0.2, 1.3)                  # dark slab, y=-2 wrld
    prop("hazard_pit", pmb)
    smb = MeshBuilder(); sk = Kit(smb); sp = sk("spikes", "spike_iron")
    for sxp in (-1.05, -0.35, 0.35, 1.05):             # symmetric cluster
        sp.cone(sxp, -0.35, 0, 0.16, 0.5, seg=6)
    prop("hazard_spikes", smb)
    for hi, (gx, gw) in enumerate(gaps):
        hx = round(gx + gw / 2, 2)
        placed.append(("Hazard_Pit_%02d" % (hi + 1), [hx, -1.5, 0], 0,
                       ["hazard", "pit"], "hazard_pit"))
        placed.append(("Hazard_Spike_%02d" % (hi + 1), [hx, -1.5, 0], 0,
                       ["hazard", "spikes"], "hazard_spikes"))

    # ---- goal flag: built at origin, mirrored cloth keeps centroid 0 ---
    fmb = MeshBuilder(); fk = Kit(fmb); pole = fk("pole", "pole_grey")
    pole.cyl(0, 0, 0, 0.06, 0.05, 3.2, seg=8)
    cloth = fk("cloth", "flag_red")
    cloth.prism(0, 2.5, 0.45, 0.05, 0.45, 0.55)
    cloth.prism(0, 2.5, -0.45, 0.05, 0.45, 0.55)
    prop("goal_flag", fmb)
    placed.append(("Goal_Flag", [round(level_len - 2, 2), 0, 0], 0,
                   ["goal", "win"], "goal_flag"))

    # ---- parallax backdrops --------------------------------------------
    for bi, (zoff, hgt, matname, suffix) in enumerate(
            [(-5.5, 7.0, "bg_near", "Near"), (-11.0, 11.0, "bg_far", "Far")]):
        bmb = MeshBuilder(); bk = Kit(bmb); bg = bk("slab", matname)
        bg.box(0, hgt / 2 - 1.0, 0, level_len / 2 + 8, hgt / 2, 0.3)
        nm = "backdrop_" + suffix.lower()
        prop(nm, bmb)
        placed.append(("Backdrop_" + suffix, [track_x, 0, zoff], 0,
                       ["backdrop", "parallax_%d" % (bi + 1)], nm))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed,
                "rusty-cinder-run")
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "rusty-cinder-run",
      # deterministic wall-clock stand-in: same seed -> byte-identical state
      "updated": (datetime.datetime(2000, 1, 1)
                  + datetime.timedelta(seconds=a.seed & 0xFFFF)
                  ).isoformat(timespec="seconds"),
      "seed": {"layout": a.seed},
      "chunk_size": 0, "radius": 0,
      "camera": {"target": [round(track_x, 2), 1.5, 0], "distance": 26},
      "chunks": [],
      "palette": MATS,
      "gameplay": {"genre": "platformer_2_5d",
                   "physics": {"gravity": GRAVITY, "jump_velocity": JUMP_V,
                               "run_speed": RUN_SPEED,
                               "max_jump_height_m": arc["peak_height_m"],
                               "max_gap_m": arc["max_range_m"],
                               "coyote_time_s": 0.10, "jump_buffer_s": 0.12},
                   "hazards": {
                     "pits": "%d gap spans marked by Hazard_Pit_* nodes "
                             "tagged ['hazard'] - falling in = instant "
                             "respawn at level start" % n_gaps,
                     "spikes": "%d spike clusters marked by Hazard_Spike_* "
                               "nodes tagged ['hazard'] on each pit floor - "
                               "touching = instant respawn" % n_gaps},
                   "scoring": {"coins": n_coins, "goal_bonus": 250}}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "RUSTY CINDER RUN 2.5D level (seed %d)" % a.seed,
               ["%d m track, %d seeded gaps (<= %.1f m jump cap), "
                "%d instanced platforms (3 deck meshes), %d coins (1 mesh), "
                "%d hazard nodes, goal flag"
                % (level_len, n_gaps, max_gap_m, len(plats), n_coins,
                   2 * n_gaps),
                "conventions: meshes at origin + node.position placement, "
                "hazards tagged ['hazard'] for LV_F_HAZARD kill semantics"])
    print("[platformer] ready: %d assets, %d scene nodes" % (len(made),
                                                             len(placed)))

if __name__ == "__main__":
    main()
