#!/usr/bin/env python3
"""ARCHETYPE ENGINE - one parametric generator covering every design_rules archetype.

Reads identity from design_rules.json, dresses the world from themes.json,
picks a layout pattern (or honors --pattern), emits assets + scene +
world_state including a structured environment block (sky/fog/wind/light).

Layout patterns:
  arena_ring   - boss arenas, bullet-hell bowls, fighting stages
  corridor_run - runners, character action, gauntlets
  hub_spoke    - open-world RPGs, metroidvania hubs, collectathons
  grid_board   - tabletop, tactics, party boards
  spline_track - racing, karts, flight courses
  room_graph   - roguelikes, dungeons, heists, survival horror wings

Usage:
  python gen_archetype.py --archetype kart_racer --theme candy_land --out-dir .
  python gen_archetype.py --list          (show archetypes + patterns)
"""
import argparse
import datetime
import json
import math
from pathlib import Path

from worldkit import (Rng, fbm, MeshBuilder, write_mtl_for, register_index,
                      write_scene, write_state, append_log, load_theme,
                      list_themes, assert_origin_centered, mesh_centroid)

HERE = Path(__file__).parent
OBJECTIVES = {
  "hub_spoke": "collect the coins, dodge the stalkers, reach the festival banner",
}
DEFAULT_MATS = {
  "structure": (0.55, 0.52, 0.48), "accent": (0.85, 0.40, 0.20),
  "ground": (0.38, 0.44, 0.34), "detail": (0.30, 0.32, 0.36),
}

# ---------------------------------------------------------------- helpers
def kit_factory(mb):
    class K:
        def __init__(self, mb): self.mb = mb
        def __call__(self, pname, mat):
            self.mb.begin(pname, mat)
            return H(self.mb)
    class H:
        def __init__(self, mb): self.mb = mb
        def box(self, *a): self.mb.box(*a)
        def cyl(self, *a, **k): self.mb.cyl(*a, **k)
        def cone(self, *a, **k): self.mb.cone(*a, **k)
        def prism(self, *a): self.mb.roof_prism(*a)
        def pyramid(self, *a): self.mb.pyramid(*a)
        def octahedron(self, *a): self.mb.octahedron(*a)
        def hex_tile(self, *a, **k): self.mb.hex_tile(*a, **k)
    return K(mb)

def emit(mb, models, name, mats, assets_dir):
    obj_text, nv, nf = mb.to_obj(name, "materials")
    (models / (name + ".obj")).write_text(obj_text, encoding="utf-8")
    register_index(assets_dir, name, "models/" + name + ".obj")
    return nf

def mat_at(mats, key, fallback):
    return key if key in mats else fallback

# ---------------------------------------------------------------- patterns
def pattern_arena(rng, mats, size=14):
    mb = MeshBuilder(); k = kit_factory(mb)
    g = k("floor", mat_at(mats, "ground", "ground")); g.cyl(0, -0.15, 0, size, size, 0.3, seg=24)
    wall = k("ring", mat_at(mats, "structure", "structure"))
    n = 16
    for i in range(n):
        a = 2.0 * math.pi * i / n
        wx, wz = size * math.cos(a), size * math.sin(a)
        wall.box(round(wx, 2), 1.0, round(wz, 2), 1.4, 1.0, 1.4)
    pill = k("pillars", mat_at(mats, "detail", "detail"))
    for i in range(6):
        a = 2.0 * math.pi * i / 6 + 0.3
        px, pz = (size - 3.5) * math.cos(a), (size - 3.5) * math.sin(a)
        pill.cyl(round(px, 2), 1.6, round(pz, 2), 0.45, 0.4, 3.2, seg=8)
        pill.octahedron(round(px, 2), 3.5, round(pz, 2), 0.5)
    cen = k("center", mat_at(mats, "accent", "accent"))
    cen.pyramid(0, 0.15, 0, 2.0, 2.0, 2.8)
    # --- game content: enemies + goal as instanced tagged nodes (props
    # built AT ORIGIN, placed purely via node position) ---
    extras = []   # (emit_name_or_None, ref_model, display_name, mb_or_None, pos, yaw, tags)
    em = MeshBuilder(); ek = kit_factory(em)
    ek("body", mat_at(mats, "detail", "detail")).box(0, 0.45, 0, 0.7, 0.45, 0.9)
    ek("crest", mat_at(mats, "accent", "accent")).pyramid(0, 0.9, 0, 0.45, 0.65, 0.55)
    assert_origin_centered(em)
    for i in range(4):
        a = math.pi / 4 * (2 * i + 1)   # diagonals: clear of pillars and center
        ex, ez = round((size / 2.0) * math.cos(a), 2), round((size / 2.0) * math.sin(a), 2)
        yaw = int(math.degrees(a + math.pi)) % 360   # face the ring center
        extras.append(("aggro_small" if i == 0 else None, "aggro_small",
                       "Aggro_Small_%02d" % (i + 1), em if i == 0 else None,
                       [ex, 0, ez], yaw, ["enemy", "aggro_small"]))
    gm = MeshBuilder(); gk = kit_factory(gm)
    gk("gem", mat_at(mats, "accent", "accent")).octahedron(0, 0, 0, 0.45)
    gk("ring", mat_at(mats, "structure", "structure")).cyl(0, -0.62, 0, 0.62, 0.62, 0.1, seg=12)
    assert_origin_centered(gm)
    extras.append(("goal_beacon", "goal_beacon", "Goal_Beacon", gm,
                   [0, 3.6, 0], 0, ["goal"]))   # hovers over the center pyramid apex
    return [("Arena_Floor", [0, 0, 0], 0, ["level"])], (mb, extras)

def pattern_corridor(rng, mats, length=70):
    mb = MeshBuilder(); k = kit_factory(mb)
    fl = k("floor", mat_at(mats, "ground", "ground"))
    fl.box(length / 2.0, -0.5, 0, length / 2.0, 0.5, 6.0)
    wl = k("walls", mat_at(mats, "structure", "structure"))
    wl.box(length / 2.0, 1.5, -3.3, length / 2.0, 1.5, 0.6)
    wl.box(length / 2.0, 1.5, 3.3, length / 2.0, 1.5, 0.6)
    ob = k("obstacles", mat_at(mats, "detail", "detail"))
    for i in range(int(length / 7)):
        ox = 6 + i * 7 + rng.uniform(-1.5, 1.5)
        if rng.uniform() > 0.5: ob.box(round(ox, 2), 0.7, round(rng.uniform(-2.2, 2.2), 2), 0.8, 0.7, 1.6)
        else: ob.cone(round(ox, 2), 0.5, round(rng.uniform(-2.2, 2.2), 2), 0.5, 1.0, seg=6)
    # --- game content: pickups + goal OUT of the baked mesh, emitted as
    # instanced tagged nodes via the hub_spoke extras convention so the
    # engine can see them. Placement math is unchanged from when these were
    # baked into layout_main; every prop mesh is built AT ORIGIN and placed
    # purely via its node position. ---
    extras = []   # (emit_name_or_None, ref_model, display_name, mb_or_None, pos, yaw, tags)
    cm = MeshBuilder(); ck = kit_factory(cm)
    ck("gem", mat_at(mats, "accent", "accent")).octahedron(0, 0, 0, 0.22)
    assert_origin_centered(cm)
    for i in range(10):
        cx = 5 + i * (length - 10) / 9.0          # same centers as the old baked run
        extras.append(("coin" if i == 0 else None, "coin", "Coin_%02d" % (i + 1),
                       cm if i == 0 else None, [cx, 1.2, 0], 0, ["pickup"]))
    gm = MeshBuilder(); gk = kit_factory(gm)
    gk("post", mat_at(mats, "accent", "accent")).box(0, 1.6, 0, 0.4, 3.2, 0.4)
    gk("bar", mat_at(mats, "structure", "structure")).box(0, 3.05, 0, 1.4, 0.12, 0.14)
    gk("cloth_l", mat_at(mats, "structure", "structure")).prism(-0.9, 2.45, 0, 0.28, 0.06, 0.55)
    gk("cloth_r", mat_at(mats, "structure", "structure")).prism(0.9, 2.45, 0, 0.28, 0.06, 0.55)
    assert_origin_centered(gm)
    extras.append(("goal_banner", "goal_banner", "Goal_Banner", gm,
                   [length - 1, 0, 0], 0, ["goal"]))
    # AUDIT 2.2 fix - CHOSEN APPROACH: pivot-relative rebuild (the
    # platformer25d track_x pattern). The level mesh used to bake world-x
    # (0..length) into floor/wall/obstacle vertices while the Corridor node
    # sat at [length/2, 0, 0]; transform-applying consumers rendered AND
    # simulated it ~2x displaced. The mesh is translated so its vertex
    # centroid hugs the origin and the NODE carries the whole placement;
    # world-space layout is unchanged (node + relative verts == old verts).
    # Identity-node fallback rejected: it would leave baked coords in the
    # OBJ and fail the centroid-at-origin gate.
    cen = mesh_centroid(mb)
    mb.translate(-cen[0], 0.0, -cen[2])
    return [("Corridor", [round(cen[0], 3), 0, round(cen[2], 3)], 0,
            ["level"])], (mb, extras)

def pattern_hub(rng, mats, spokes=5, reach=26):
    mb = MeshBuilder(); k = kit_factory(mb)
    pl = k("plaza", mat_at(mats, "structure", "structure"))
    pl.cyl(0, 0.05, 0, 7, 7, 0.35, seg=20)
    pa = k("paths", mat_at(mats, "ground", "ground"))
    for s in range(spokes):
        a = 2.0 * math.pi * s / spokes
        dx, dz = math.cos(a), math.sin(a)
        for t in range(4, reach, 3):
            pa.box(round(dx * t, 2), 0.08, round(dz * t, 2), 1.5, 0.16, 1.5)
    # POI stones: built AT ORIGIN (audit item 6 - they used to bake the
    # spoke-end coords into vertices AND set node.position to the same
    # numbers, double-transforming under transform-applying consumers);
    # the node position alone carries placement now.
    poi = []
    for s in range(spokes):
        a = 2.0 * math.pi * s / spokes
        ex, ez = math.cos(a) * reach, math.sin(a) * reach
        pm = MeshBuilder(); pk = kit_factory(pm)
        st = pk("stone", mat_at(mats, "detail", "detail"))
        st.cyl(0, 0.9, 0, 1.0, 0.8, 1.8, seg=8)
        top = pk("cap", mat_at(mats, "accent", "accent"))
        top.octahedron(0, 2.3, 0, 0.7)
        assert_origin_centered(pm)
        poi.append((pm, "POI_%02d" % (s + 1), [round(ex, 2), 0, round(ez, 2)]))
    dec = k("scatter", mat_at(mats, "detail", "detail"))
    for i in range(14):
        a = rng.uniform(0, 2.0 * math.pi); rr = rng.uniform(11, reach + 6)
        x, z = rr * math.cos(a), rr * math.sin(a)
        near_path = False
        for s in range(spokes):
            sa = 2.0 * math.pi * s / spokes
            proj = x * math.cos(sa) + z * math.sin(sa)
            perp = abs(-x * math.sin(sa) + z * math.cos(sa))
            if 0 < proj < reach and perp < 3.0:
                near_path = True
                break
        if near_path:
            continue
        dec.cone(round(x, 2), 1.0, round(z, 2), 0.9, 2.0, seg=7)
    # --- game content: coins, stalkers, goal banner ---
    extras = []   # (emit_name_or_None, ref_model, display_name, mb_or_None, pos, yaw, tags)
    cm = MeshBuilder(); ck = kit_factory(cm)
    ck("gem", mat_at(mats, "accent", "accent")).octahedron(0, 0.55, 0, 0.28)
    assert_origin_centered(cm)
    extras.append(("coin", "coin", "Coin", cm, [0, 0, 0], 0, ["pickup"]))
    coin_n = 0
    for i in range(8):
        a = math.pi / 4 * i
        coin_n += 1
        extras.append((None, "coin", "Coin_%02d" % coin_n, None,
                       [round(10 * math.cos(a), 2), 0, round(10 * math.sin(a), 2)],
                       int(a * 57.3) % 360, ["pickup"]))
    for s in range(3):
        a2 = 2.0 * math.pi * s / spokes
        for t in (13, 21):
            coin_n += 1
            extras.append((None, "coin", "Coin_%02d" % coin_n, None,
                           [round(math.cos(a2) * t, 2), 0, round(math.sin(a2) * t, 2)],
                           int(a2 * 57.3) % 360, ["pickup"]))
    sm = MeshBuilder(); sk = kit_factory(sm)
    sk("body", mat_at(mats, "detail", "detail")).box(0, 0.9, 0, 0.5, 1.8, 0.5)
    sk("hood", mat_at(mats, "accent", "accent")).pyramid(0, 1.9, 0, 0.6, 0.6, 0.7)
    assert_origin_centered(sm)
    extras.append(("stalker", "stalker", "Stalker", sm, [0, 0, 0], 0, ["enemy"]))
    for i in range(2):
        a3 = 2.0 * math.pi * (i + 0.5) / spokes
        extras.append((None, "stalker", "Stalker_%02d" % (i + 1), None,
                       [round(16 * math.cos(a3), 2), 0, round(16 * math.sin(a3), 2)], 0, ["enemy"]))
    # festival banner: AT ORIGIN (audit item 6 - pole/cloth used to bake
    # reach+4 into vertices while the node sat at [0, 0, 0]); the node
    # position now carries the spoke-0 end placement. Cloth re-centered on
    # the pole so the mesh hugs the origin per the transform convention.
    bm = MeshBuilder(); bk = kit_factory(bm)
    bk("pole", mat_at(mats, "structure", "structure")).cyl(0, 0.0, 0, 0.09, 0.07, 4.6, seg=8)
    bk("cloth", mat_at(mats, "accent", "accent")).prism(0, 3.4, 0, 0.06, 0.7, 1.1)
    assert_origin_centered(bm)
    extras.append(("festival_banner", "festival_banner", "Festival_Banner", bm,
                   [reach + 4, 0, 0], 0, ["goal"]))
    placed = [("Central_Plaza", [0, 0, 0], 0, ["hub"])]
    return placed, (mb, poi, extras)

def pattern_board(rng, mats, n=4):
    mb = MeshBuilder(); k = kit_factory(mb)
    fr = k("frame", mat_at(mats, "structure", "structure"))
    fr.box(0, -0.12, 0, 5.6, 0.1, 5.6)
    kinds = sorted(set(mats.keys()) - {"structure"}) or ["ground"]
    for qx in range(-n, n + 1):
        for qz in range(-n, n + 1):
            band = int(fbm(qx * 0.5 + 7, qz * 0.5 + 7, rng.next_u32()) * len(kinds))
            band = min(band, len(kinds) - 1)
            t = k("t", kinds[band])
            t.hex_tile(qx * 1.15, 0, qz * 1.15, 0.56, 0.14)
    placed = [("Board", [0, 0, 0], 0, ["board"])]
    # --- game content: pawns on the near rank + goal gate on the far edge
    # (props built AT ORIGIN, placed purely via node position) ---
    extras = []   # (emit_name_or_None, ref_model, display_name, mb_or_None, pos, yaw, tags)
    tile_top = 0.14   # hex_tile(cx, 0, cz, 0.56, 0.14) top surface
    pm = MeshBuilder(); pk = kit_factory(pm)
    pk("base", mat_at(mats, "detail", "detail")).cyl(0, 0, 0, 0.15, 0.11, 0.42, seg=8)
    pk("collar", mat_at(mats, "detail", "detail")).cyl(0, 0.42, 0, 0.10, 0.02, 0.10, seg=8)
    assert_origin_centered(pm)
    for j, qx in enumerate(range(-2, 3)):
        px, pz = round(qx * 1.15, 2), round((n - 1) * 1.15, 2)
        extras.append(("pawn" if j == 0 else None, "pawn", "Pawn_%02d" % (j + 1),
                       pm if j == 0 else None, [px, tile_top, pz], 0,
                       ["enemy", "piece"]))   # enemy = engine-visible; piece = tabletop flavor
    gm = MeshBuilder(); gk = kit_factory(gm)
    gk("post_l", mat_at(mats, "structure", "structure")).box(-1.0, 0.55, 0, 0.14, 0.55, 0.14)
    gk("post_r", mat_at(mats, "structure", "structure")).box(1.0, 0.55, 0, 0.14, 0.55, 0.14)
    gk("lintel", mat_at(mats, "accent", "accent")).box(0, 1.16, 0, 1.12, 0.08, 0.16)
    assert_origin_centered(gm)
    extras.append(("goal_gate", "goal_gate", "Goal_Gate", gm,
                   [0, tile_top, -round(n * 1.15, 2)], 0, ["goal"]))
    return placed, (mb, extras)

def _catmull(p0, p1, p2, p3, t):
    t2 = t * t; t3 = t2 * t
    return tuple(0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3) for i in range(2))

# --- spline input conditioning (audit item 5): duplicate / near-duplicate
# control points and collinear runs used to be able to degenerate the closed
# Catmull-Rom loop (zero-length segments); every stage below is guarded ---

TRACK_CTRL_MIN_SEP = 0.5   # m; control points closer than this are dropped
SAMPLE_MIN_SEG = 0.05      # m; sampled segments shorter than this are skipped
TRACK_MAX_SAMPLES = 240    # hard clamp on emitted road samples

def condition_points(points, min_sep):
    """Deterministic input conditioning: drop non-finite points and any
    point closer than min_sep to the last accepted one (first of every
    near-duplicate run wins). Never raises on degenerate input."""
    out = []
    for p in points:
        if not (math.isfinite(p[0]) and math.isfinite(p[1])):
            continue
        if out and math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) < min_sep:
            continue
        out.append((float(p[0]), float(p[1])))
    return out

def unit_tangent(p, q):
    """Normalized p->q direction, or None when the segment is degenerate
    (zero-length or non-finite) - callers skip instead of dividing by ~0."""
    dx, dz = q[0] - p[0], q[1] - p[1]
    ln = math.hypot(dx, dz)
    if not math.isfinite(ln) or ln < 1e-9:
        return None
    return (dx / ln, dz / ln)

def thin_polyline(samples, min_seg=SAMPLE_MIN_SEG, cap=TRACK_MAX_SAMPLES):
    """Condition a sampled polyline: finite points only, zero-length
    segments removed via the guarded unit tangent, count clamped to cap."""
    kept = []
    for p in samples:
        if len(kept) >= cap:
            break
        if not (math.isfinite(p[0]) and math.isfinite(p[1])):
            continue
        if kept and unit_tangent(kept[-1], p) is None:
            continue
        kept.append((float(p[0]), float(p[1])))
    return kept

def track_control_points(rng, count=6, spread=30.0, attempts=8):
    """Seeded control points conditioned to >=4 accepted points
    TRACK_CTRL_MIN_SEP apart; seeded retries first, then a canonical ring
    fallback that is well-conditioned by construction."""
    for _ in range(attempts):
        pts = condition_points(
            [(rng.uniform(-spread, spread), rng.uniform(-spread, spread))
             for _ in range(count)], TRACK_CTRL_MIN_SEP)
        if len(pts) >= 4:
            return pts
    return [(round(spread * 0.8 * math.cos(2.0 * math.pi * i / count), 3),
             round(spread * 0.8 * math.sin(2.0 * math.pi * i / count), 3))
            for i in range(count)]

def sample_closed_spline(ctrl, per_segment=10):
    """Closed Catmull-Rom through CONDITIONED control points; degenerate
    samples skipped by thin_polyline, segment count clamped. Finite output
    for any finite control input."""
    n = len(ctrl)
    raw = []
    for i in range(n):
        p0, p1 = ctrl[(i - 1) % n], ctrl[i]
        p2, p3 = ctrl[(i + 1) % n], ctrl[(i + 2) % n]
        for s in range(per_segment):
            raw.append(_catmull(p0, p1, p2, p3, s / float(per_segment)))
    return thin_polyline(raw)

def pattern_track(rng, mats):
    pts = sample_closed_spline(track_control_points(rng))
    mb = MeshBuilder(); k = kit_factory(mb)
    rd = k("road", mat_at(mats, "ground", "ground"))
    for x, z in pts:
        rd.box(round(x, 2), 0.06, round(z, 2), 0.95, 0.12, 0.95)
    cp = k("checkpoints", mat_at(mats, "accent", "accent"))
    step = max(1, len(pts) // 4)   # guard: range() step must stay >= 1
    for j in range(0, len(pts), step):
        x, z = pts[j]
        cp.box(round(x, 2), 1.2, round(z, 2), 0.3, 2.4, 0.3)
    # start gate: its OWN mesh so the Start_Line node's model:start_line
    # reference resolves (dangling refs break runtimes). Built AT ORIGIN -
    # audit item 6 class fix: the gate used to bake sx/sz into its vertices
    # while the Start_Line node carried the same coords (double transform).
    sx, sz = round(pts[0][0], 2), round(pts[0][1], 2)
    sm = MeshBuilder(); sk = kit_factory(sm)
    posts = sk("gate_posts", mat_at(mats, "structure", "structure"))
    posts.box(-1.6, 1.5, 0, 0.28, 3.0, 0.28)
    posts.box(1.6, 1.5, 0, 0.28, 3.0, 0.28)
    bar = sk("gate_bar", mat_at(mats, "accent", "accent"))
    bar.box(0, 3.05, 0, 3.6, 0.34, 0.3)
    assert_origin_centered(sm)
    placed = [("Track_Loop", [0, 0, 0], 0, ["track"]),
              ("Start_Line", [sx, 0, sz], 0, ["start", "poi"])]
    return placed, (mb, sm)

# --- room_graph connectivity (audit item 4): picking rooms with
# rng.uniform() > 0.25 never guaranteed corridor reachability - isolated
# rooms left loot unreachable. These helpers plan doorways + L-shaped
# linking corridors over room centers and PROVE reachability by BFS before
# any geometry is emitted. ---

def _edge_key(a, b):
    """Canonical undirected key for a cell-pair edge (wall doorway)."""
    return (a, b) if a <= b else (b, a)

def choose_rooms(rng, cols=3, rows=3, keep_above=0.25):
    """Seed-selected room cells in scan order (same draws as the original
    inline picker, extracted so probes can reproduce the selection)."""
    return [(cx, cz) for cx in range(cols) for cz in range(rows)
            if rng.uniform() > keep_above]

def corridor_cells(a, b):
    """Cell-wise L-path a -> b (x leg first, then z), endpoints included.
    Legs always run along room-center rows/columns, so every wall crossing
    lands inside that wall's standard central door gap."""
    out = [a]
    cx, cz = a
    while cx != b[0]:
        cx += 1 if b[0] > cx else -1
        out.append((cx, cz))
    while cz != b[1]:
        cz += 1 if b[1] > cz else -1
        out.append((cx, cz))
    return out

def plan_room_connectivity(chosen):
    """Doorways + corridor links making every chosen room reachable.

    Returns (portals, links): portals is the set of normalized cell-pair
    edges that must ship with a wall gap (adjacent chosen rooms plus every
    perimeter a linking corridor threads); links is a minimal list of
    (i, j) room-index pairs needing an L-corridor, built by greedy
    closest-pair merging of connected components (= MST over room centers,
    ties broken deterministically by index)."""
    n = len(chosen)
    idx = {}
    for i, c in enumerate(chosen):
        idx[c] = i
    portals = set()
    adj = [[] for _ in range(n)]
    for i, (cx, cz) in enumerate(chosen):
        for nb in ((cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1)):
            j = idx.get(nb)
            if j is not None:
                portals.add(_edge_key((cx, cz), nb))
                adj[i].append(j)
    comp = [-1] * n
    ncomp = 0
    for s in range(n):
        if comp[s] != -1:
            continue
        comp[s] = ncomp
        stack = [s]
        while stack:
            u = stack.pop()
            for v in adj[u]:
                if comp[v] == -1:
                    comp[v] = ncomp
                    stack.append(v)
        ncomp += 1
    links = []
    while ncomp > 1:
        best = None
        for i in range(n):
            for j in range(i + 1, n):
                if comp[i] == comp[j]:
                    continue
                d = (abs(chosen[i][0] - chosen[j][0])
                     + abs(chosen[i][1] - chosen[j][1]))
                key = (d, i, j)
                if best is None or key < best:
                    best = key
        _, i, j = best
        links.append((i, j))
        dead, live = comp[j], comp[i]
        for k in range(n):
            if comp[k] == dead:
                comp[k] = live
        ncomp -= 1
    return portals, links

def room_reachability(chosen, portals, links):
    """BFS over the room+corridor adjacency about to be emitted. Rooms are
    nodes 0..n-1; every link adds one corridor node joined to both endpoint
    rooms and to any chosen room whose wall the L-path threads. Returns the
    sorted list of reached room ids; callers assert full coverage BEFORE
    emitting geometry."""
    n = len(chosen)
    idx = {}
    for i, c in enumerate(chosen):
        idx[c] = i
    cset = set(chosen)
    graph = [[] for _ in range(n + len(links))]
    for a, b in portals:
        if a in idx and b in idx:
            graph[idx[a]].append(idx[b])
            graph[idx[b]].append(idx[a])
    for ci, (i, j) in enumerate(links):
        cn = n + ci
        graph[i].append(cn)
        graph[cn].append(i)
        graph[j].append(cn)
        graph[cn].append(j)
        prev = None
        for cell in corridor_cells(chosen[i], chosen[j]):
            if prev is not None and (prev in cset or cell in cset):
                for c in (prev, cell):
                    if c in idx:
                        graph[idx[c]].append(cn)
                        graph[cn].append(idx[c])
            prev = cell
    seen = [False] * len(graph)
    seen[0] = True
    queue = [0]
    head = 0
    while head < len(queue):
        u = queue[head]
        head += 1
        for v in graph[u]:
            if not seen[v]:
                seen[v] = True
                queue.append(v)
    return sorted(i for i in range(n) if seen[i])

def pattern_rooms(rng, mats, cols=3, rows=3):
    mb = MeshBuilder(); k = kit_factory(mb)
    CW, RW, RW2 = 12.0, 9.0, 9.0
    chosen = choose_rooms(rng, cols, rows)
    if not chosen:                       # pathological all-walls roll
        chosen = [(cols // 2, rows // 2)]
    portals, links = plan_room_connectivity(chosen)
    # reachability proof BEFORE emitting: planning is constructive, this is
    # the seatbelt assert demanded by the audit item
    reached = room_reachability(chosen, portals, links)
    if len(reached) != len(chosen):
        raise RuntimeError(
            "room_graph: planned layout leaves rooms unreachable: %r of %r"
            % (reached, chosen))
    cset = set(chosen)
    fl = k("floors", mat_at(mats, "ground", "ground"))
    for cx, cz in chosen:
        x0, z0 = cx * CW, cz * RW2
        fl.box(x0 + CW / 2.0, 0.05, z0 + RW2 / 2.0, CW / 2.0 - 0.5, 0.1, RW2 / 2.0 - 0.5)
    # linking corridors: 1.8 m walkway tiles flush with the room floors;
    # every threaded perimeter registers a doorway portal consumed below
    corr = 0.9                            # half-width of corridor tiles
    for i, j in links:
        prev = None
        for cell in corridor_cells(chosen[i], chosen[j]):
            if prev is not None:
                if prev in cset or cell in cset:
                    portals.add(_edge_key(prev, cell))
                px, pz = prev[0] * CW + CW / 2.0, prev[1] * RW2 + RW2 / 2.0
                qx, qz = cell[0] * CW + CW / 2.0, cell[1] * RW2 + RW2 / 2.0
                fl.box(round((px + qx) / 2.0, 2), 0.05, round((pz + qz) / 2.0, 2),
                       round(abs(qx - px) / 2.0 + corr, 2), 0.1,
                       round(abs(qz - pz) / 2.0 + corr, 2))
            prev = cell
    wa = k("walls", mat_at(mats, "structure", "structure"))
    for cx, cz in chosen:
        x0, z0 = cx * CW, cz * RW2
        for side, (nx, nz) in enumerate([(cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1)]):
            door = _edge_key((cx, cz), (nx, nz)) in portals
            if side == 0:
                wx = x0 + CW
                if door:
                    wa.box(wx, 1.2, z0 + RW2 * 0.25, 0.3, 1.2, RW2 * 0.25)
                    wa.box(wx, 1.2, z0 + RW2 * 0.75, 0.3, 1.2, RW2 * 0.25)
                else:
                    wa.box(wx, 1.2, z0 + RW2 / 2.0, 0.3, 1.2, RW2 / 2.0)
            elif side == 1:
                wx = x0
                if door:
                    wa.box(wx, 1.2, z0 + RW2 * 0.25, 0.3, 1.2, RW2 * 0.25)
                    wa.box(wx, 1.2, z0 + RW2 * 0.75, 0.3, 1.2, RW2 * 0.25)
                else:
                    wa.box(wx, 1.2, z0 + RW2 / 2.0, 0.3, 1.2, RW2 / 2.0)
            elif side == 2:
                wz = z0 + RW2
                if door:
                    wa.box(x0 + CW * 0.25, 1.2, wz, CW * 0.25, 1.2, 0.3)
                    wa.box(x0 + CW * 0.75, 1.2, wz, CW * 0.25, 1.2, 0.3)
                else:
                    wa.box(x0 + CW / 2.0, 1.2, wz, CW / 2.0, 1.2, 0.3)
            else:
                wz = z0
                if door:
                    wa.box(x0 + CW * 0.25, 1.2, wz, CW * 0.25, 1.2, 0.3)
                    wa.box(x0 + CW * 0.75, 1.2, wz, CW * 0.25, 1.2, 0.3)
                else:
                    wa.box(x0 + CW / 2.0, 1.2, wz, CW / 2.0, 1.2, 0.3)
    # gameplay wiring via the item-3 extras convention: baked-in loot
    # octahedra become instanced pickup nodes the engine can see (baseline
    # littcli validate scored interactives:0 / ok:false without this), plus
    # one goal-tagged exit gate in the last chosen room. Prop meshes are
    # built AT ORIGIN; node positions carry placement only.
    extras = []
    looted = []
    gm = None
    for idx, (cx, cz) in enumerate(chosen):
        x0, z0 = cx * CW, cz * RW2
        if rng.uniform() > 0.5 or idx == len(chosen) - 1:
            lx = x0 + CW * rng.uniform(0.3, 0.7); lz = z0 + RW2 * rng.uniform(0.3, 0.7)
            if gm is None:
                gm = MeshBuilder(); gk = kit_factory(gm)
                gk("gem", mat_at(mats, "accent", "accent")).octahedron(0, 0.8, 0, 0.35)
                assert_origin_centered(gm)
            extras.append(("loot_gem" if not looted else None, "loot_gem",
                           "Loot_Gem_%02d" % (len(looted) + 1),
                           gm if not looted else None,
                           [round(lx, 2), 0, round(lz, 2)], 0, ["pickup"]))
            looted.append(idx)
    xm = MeshBuilder(); xk = kit_factory(xm)
    xp = xk("exit_frame", mat_at(mats, "structure", "structure"))
    xp.box(-0.8, 0.9, 0, 0.18, 0.9, 0.18)
    xp.box(0.8, 0.9, 0, 0.18, 0.9, 0.18)
    xl = xk("exit_lintel", mat_at(mats, "accent", "accent"))
    xl.box(0, 1.85, 0, 0.95, 0.12, 0.2)
    xs = xk("exit_core", mat_at(mats, "accent", "accent"))
    xs.octahedron(0, 1.1, 0, 0.45)
    assert_origin_centered(xm)
    ex, ez = chosen[-1][0] * CW + CW / 2.0, chosen[-1][1] * RW2 + RW2 / 2.0
    extras.append(("exit_gate", "exit_gate", "Exit_Gate", xm,
                   [round(ex, 2), 0, round(ez, 2)], 0, ["goal"]))
    # AUDIT 2.2 fix - same pivot-relative approach as corridor_run (see the
    # comment there): floors/walls/corridors used to bake cell-offset coords
    # while the Dungeon node carried [cols*CW/2, 0, rows*RW2/2] - a double
    # transform. The mesh re-centers on its OWN vertex centroid (chosen
    # rooms can cluster off grid-center, so the centroid - not the grid
    # middle - is the honest pivot) and the node carries that placement.
    cen = mesh_centroid(mb)
    mb.translate(-cen[0], 0.0, -cen[2])
    placed = [("Dungeon", [round(cen[0], 3), 0, round(cen[2], 3)], 0,
               ["level"])]
    return placed, (mb, extras)

PATTERNS = {
  "arena_ring": pattern_arena, "corridor_run": pattern_corridor,
  "hub_spoke": pattern_hub, "grid_board": pattern_board,
  "spline_track": pattern_track, "room_graph": pattern_rooms,
}

STRUCTURE_HINTS = [
  (("procedural", "dungeon"), "room_graph"),
  (("hub_spoke", "semi_open", "metroidvania"), "hub_spoke"),
  (("mission_based", "arenas", "wave_based"), "arena_ring"),
  (("infinite", "linear"), "corridor_run"),
]

def guess_pattern(rules):
    s = rules.get("structure") or ""
    for keys, pat in STRUCTURE_HINTS:
        if any(k in s for k in keys): return pat
    return "arena_ring"

# ------------------------------------------------------------- environment
TIME_PRESETS = {
  "dawn":   {"sky_type": "sunrise", "top": [0.30, 0.40, 0.65], "hor": [0.95, 0.65, 0.45], "sun_el": 12, "amb": [0.45, 0.40, 0.42], "intensity": 0.8},
  "noon":   {"sky_type": "clear", "top": [0.35, 0.55, 0.90], "hor": [0.75, 0.85, 0.95], "sun_el": 62, "amb": [0.50, 0.52, 0.55], "intensity": 1.0},
  "dusk":   {"sky_type": "sunset", "top": [0.20, 0.22, 0.45], "hor": [0.90, 0.45, 0.30], "sun_el": 8, "amb": [0.40, 0.33, 0.35], "intensity": 0.7},
  "night":  {"sky_type": "night", "top": [0.03, 0.04, 0.10], "hor": [0.10, 0.12, 0.22], "sun_el": -8, "amb": [0.12, 0.14, 0.20], "intensity": 0.25},
}
WEATHER_PRESETS = {
  "clear": {"fog_d": 0.010, "wind": 0.25, "part": 0.0},
  "rain":  {"fog_d": 0.035, "wind": 0.80, "part": 0.7},
  "snow":  {"fog_d": 0.045, "wind": 0.45, "part": 0.6},
  "fog":   {"fog_d": 0.090, "wind": 0.10, "part": 0.0},
  "storm": {"fog_d": 0.060, "wind": 1.00, "part": 1.0},
}

def env_block(time_of_day, weather, sun_azimuth, theme_data):
    tp = TIME_PRESETS.get(time_of_day, TIME_PRESETS["noon"])
    wp = WEATHER_PRESETS.get(weather, WEATHER_PRESETS["clear"])
    return {
      "sky": {"type": tp["sky_type"], "top_color": tp["top"], "horizon_color": tp["hor"],
              "cloud_layers": [{"density": 0.4 if weather != "clear" else 0.15, "height": 120, "speed": wp["wind"]}]},
      "sun": {"azimuth_deg": sun_azimuth, "elevation_deg": tp["sun_el"]},
      "lighting": {"global_light_intensity": tp["intensity"], "ambient_light_color": tp["amb"],
                   "shadow_softness": 0.5, "time_of_day": time_of_day},
      "fog": {"density": wp["fog_d"], "height": 30, "haze": wp["fog_d"] * 40},
      "weather": {"type": weather, "wind_speed": wp["wind"], "particle_density": wp["part"]},
      "notes": list(theme_data.get("env_notes", [])),
    }

# ------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archetype", default=None)
    ap.add_argument("--pattern", default=None, choices=sorted(PATTERNS))
    ap.add_argument("--theme", default=None)
    ap.add_argument("--name", default=None)
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--seed", type=int, default=1337)
    ap.add_argument("--time-of-day", default="noon", choices=sorted(TIME_PRESETS))
    ap.add_argument("--weather", default="clear", choices=sorted(WEATHER_PRESETS))
    ap.add_argument("--sun-azimuth", type=float, default=135.0)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    rules = json.loads((HERE / "design_rules.json").read_text(encoding="utf-8"))["archetypes"]
    themes_avail = list_themes()
    if a.list or not a.archetype:
        print("archetypes (%d): %s" % (len(rules), ", ".join(sorted(rules))))
        print("patterns: %s" % ", ".join(sorted(PATTERNS)))
        print("themes (%d): %s" % (len(themes_avail), ", ".join(themes_avail)))
        return
    if a.archetype not in rules:
        raise SystemExit("unknown archetype %r - see --list" % a.archetype)
    rules_a = rules[a.archetype]

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"

    mats = dict(DEFAULT_MATS)
    theme_data = {}
    if a.theme:
        theme_data = load_theme(a.theme)
        mats = {k: tuple(v) for k, v in theme_data["palette"].items()}
        mats.setdefault("ground", (0.38, 0.44, 0.34))
    write_mtl_for(models, "materials", mats)

    pattern = a.pattern or guess_pattern(rules_a)
    rng = Rng(a.seed)
    result = PATTERNS[pattern](rng, mats)
    base_placed, payload = result
    extras = []
    extra_prop = None
    if pattern == "hub_spoke":
        main_mb, poi_list, extras = payload
    elif pattern == "spline_track":
        main_mb, extra_prop = payload   # spline_track start gate mesh
    elif isinstance(payload, tuple):
        main_mb, extras = payload   # arena_ring / corridor_run / grid_board gameplay props
    else:
        main_mb = payload
    made = []
    nf = emit(main_mb, models, "layout_main", mats, assets_dir)
    made.append(("layout_main.obj", nf))
    if extra_prop is not None:
        gnf = emit(extra_prop, models, "start_line", mats, assets_dir)
        made.append(("start_line.obj", gnf))
    placed = list(base_placed)
    # every placed node MUST carry a model:<file> tag or the play runtime
    # cannot instantiate it - patterns declare geometry, we wire the tag here
    name0, pos0, yaw0, tags0 = placed[0]
    placed[0] = (name0, pos0, yaw0, list(tags0) + ["model:layout_main"])
    if pattern == "hub_spoke":
        for pm, nm, pos in poi_list:
            nf = emit(pm, models, nm.lower(), mats, assets_dir)
            made.append((nm.lower() + ".obj", nf))
            placed.append((nm, pos, 0, ["poi", "model:" + nm.lower()]))
    for ename, rname, disp, mbx, epos, eyaw, etags in extras:
        if ename is not None:
            nf = emit(mbx, models, ename, mats, assets_dir)
            made.append((ename + ".obj", nf))
        placed.append((disp, epos, eyaw, list(etags) + ["model:" + rname]))
    if pattern in ("hub_spoke", "arena_ring", "spline_track"):
        gm = MeshBuilder(); gk = kit_factory(gm)
        gpad = gk("pad", mat_at(mats, "ground", "ground"))
        gpad.box(0, -0.15, 0, 80, 0.3, 80)
        gnf = emit(gm, models, "ground_pad", mats, assets_dir)
        made.append(("ground_pad.obj", gnf))
        placed.append(("Ground_Pad", [0, 0, 0], 0, ["floor", "model:ground_pad"]))

    scene_title = a.name or ("%s-%s" % (a.archetype.replace("_", "-"), pattern.replace("_", "-")))
    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, scene_title)

    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": a.theme or scene_title,
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"archetype_seed": a.seed},
      "chunk_size": 0, "radius": 0,
      "camera": {"target": [0, 1, 0], "distance": 30},
      "chunks": [],
      "palette": mats,
      "identity": {"archetype": a.archetype, "camera": rules_a.get("camera"),
                   "combat": rules_a.get("combat"), "movement": rules_a.get("movement"),
                   "structure": rules_a.get("structure"), "pacing": rules_a.get("pacing"),
                   "pattern": pattern, "theme": a.theme},
      "environment": env_block(a.time_of_day, a.weather, a.sun_azimuth, theme_data),
      "gameplay": {"genre": a.archetype,
                   "objective": OBJECTIVES.get(pattern, "explore and complete the archetype goals"),
                   "procgen_rules": rules_a.get("procgen_rules"),
                   "ai_behavior": rules_a.get("ai_behavior"),
                   "environment_types": rules_a.get("environment_types")},
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "%s world via %s (theme %s, seed %d)" % (a.archetype, pattern, a.theme or "default", a.seed),
               ["%d assets emitted; environment block: %s/%s" % (len(made), a.time_of_day, a.weather),
                "identity copied into state.identity from design_rules.json"])
    print("[archetype] %s | %s | %s -> %d assets, %d nodes" % (a.archetype, pattern, a.theme, len(made), len(placed)))

if __name__ == "__main__":
    main()
