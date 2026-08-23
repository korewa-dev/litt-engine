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
                      write_scene, write_state, append_log, load_theme, list_themes)

HERE = Path(__file__).parent
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
    return [("Arena_Floor", [0, 0, 0], 0, ["level"])], mb

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
    co = k("pickups", mat_at(mats, "accent", "accent"))
    for i in range(10):
        co.octahedron(5 + i * (length - 10) / 9.0, 1.2, 0, 0.22)
    po = k("goal", mat_at(mats, "accent", "accent"))
    po.box(length - 1, 1.6, 0, 0.4, 3.2, 0.4)
    return [("Corridor", [length / 2.0, 0, 0], 0, ["level"])], mb

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
    poi = []
    for s in range(spokes):
        a = 2.0 * math.pi * s / spokes
        ex, ez = math.cos(a) * reach, math.sin(a) * reach
        pm = MeshBuilder(); pk = kit_factory(pm)
        st = pk("stone", mat_at(mats, "detail", "detail"))
        st.cyl(ex, 0.9, ez, 1.0, 0.8, 1.8, seg=8)
        top = pk("cap", mat_at(mats, "accent", "accent"))
        top.octahedron(ex, 2.3, ez, 0.7)
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
    placed = [("Central_Plaza", [0, 0, 0], 0, ["hub"])]
    return placed, (mb, poi)

def pattern_board(rng, mats, n=4):
    mb = MeshBuilder(); k = kit_factory(mb)
    fr = k("frame", mat_at(mats, "structure", "structure"))
    fr.box(0, -0.12, 0, 5.6, 0.1, 5.6)
    tl = k("tiles", "")
    kinds = sorted(set(mats.keys()) - {"structure"}) or ["ground"]
    for qx in range(-n, n + 1):
        for qz in range(-n, n + 1):
            band = int(fbm(qx * 0.5 + 7, qz * 0.5 + 7, rng.next_u32()) * len(kinds))
            band = min(band, len(kinds) - 1)
            t = k("t", kinds[band])
            t.hex_tile(qx * 1.15, 0, qz * 1.15, 0.56, 0.14)
    placed = [("Board", [0, 0, 0], 0, ["board"])]
    return placed, mb

def _catmull(p0, p1, p2, p3, t):
    t2 = t * t; t3 = t2 * t
    return tuple(0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3) for i in range(2))

def pattern_track(rng, mats):
    ctrl = [(rng.uniform(-30, 30), rng.uniform(-30, 30)) for _ in range(6)]
    pts = []
    for i in range(6):
        p0, p1, p2, p3 = ctrl[(i - 1) % 6], ctrl[i], ctrl[(i + 1) % 6], ctrl[(i + 2) % 6]
        for s in range(10):
            pts.append(_catmull(p0, p1, p2, p3, s / 10.0))
    mb = MeshBuilder(); k = kit_factory(mb)
    rd = k("road", mat_at(mats, "ground", "ground"))
    for j, (x, z) in enumerate(pts):
        rd.box(round(x, 2), 0.06, round(z, 2), 0.95, 0.12, 0.95)
    cp = k("checkpoints", mat_at(mats, "accent", "accent"))
    for j in range(0, len(pts), len(pts) // 4):
        x, z = pts[j]
        cp.box(round(x, 2), 1.2, round(z, 2), 0.3, 2.4, 0.3)
    placed = [("Track_Loop", [0, 0, 0], 0, ["track"]), ("Start_Line", [round(pts[0][0], 2), 0, round(pts[0][1], 2)], 0, ["start"])]
    return placed, mb

def pattern_rooms(rng, mats, cols=3, rows=3):
    mb = MeshBuilder(); k = kit_factory(mb)
    CW, RW, RW2 = 12.0, 9.0, 9.0
    chosen = [(cx, cz) for cx in range(cols) for cz in range(rows) if rng.uniform() > 0.25]
    fl = k("floors", mat_at(mats, "ground", "ground"))
    for cx, cz in chosen:
        x0, z0 = cx * CW, cz * RW2
        fl.box(x0 + CW / 2.0, 0.05, z0 + RW2 / 2.0, CW / 2.0 - 0.5, 0.1, RW2 / 2.0 - 0.5)
    wa = k("walls", mat_at(mats, "structure", "structure"))
    looted = []
    for idx, (cx, cz) in enumerate(chosen):
        x0, z0 = cx * CW, cz * RW2
        for side, (nx, nz) in enumerate([(cx + 1, cz), (cx - 1, cz), (cx, cz + 1), (cx, cz - 1)]):
            linked = (nx, nz) in chosen
            mid = 1.5
            segs = [(-mid + 0.0)] if False else None
            if side == 0:
                wx = x0 + CW
                if linked:
                    wa.box(wx, 1.2, z0 + RW2 * 0.25, 0.3, 1.2, RW2 * 0.25)
                    wa.box(wx, 1.2, z0 + RW2 * 0.75, 0.3, 1.2, RW2 * 0.25)
                else:
                    wa.box(wx, 1.2, z0 + RW2 / 2.0, 0.3, 1.2, RW2 / 2.0)
            elif side == 1:
                wx = x0
                if linked:
                    wa.box(wx, 1.2, z0 + RW2 * 0.25, 0.3, 1.2, RW2 * 0.25)
                    wa.box(wx, 1.2, z0 + RW2 * 0.75, 0.3, 1.2, RW2 * 0.25)
                else:
                    wa.box(wx, 1.2, z0 + RW2 / 2.0, 0.3, 1.2, RW2 / 2.0)
            elif side == 2:
                wz = z0 + RW2
                if linked:
                    wa.box(x0 + CW * 0.25, 1.2, wz, CW * 0.25, 1.2, 0.3)
                    wa.box(x0 + CW * 0.75, 1.2, wz, CW * 0.25, 1.2, 0.3)
                else:
                    wa.box(x0 + CW / 2.0, 1.2, wz, CW / 2.0, 1.2, 0.3)
            else:
                wz = z0
                if linked:
                    wa.box(x0 + CW * 0.25, 1.2, wz, CW * 0.25, 1.2, 0.3)
                    wa.box(x0 + CW * 0.75, 1.2, wz, CW * 0.25, 1.2, 0.3)
                else:
                    wa.box(x0 + CW / 2.0, 1.2, wz, CW / 2.0, 1.2, 0.3)
        if rng.uniform() > 0.5 or idx == len(chosen) - 1:
            lt = k("loot_" + str(idx), mat_at(mats, "accent", "accent"))
            lx = x0 + CW * rng.uniform(0.3, 0.7); lz = z0 + RW2 * rng.uniform(0.3, 0.7)
            lt.octahedron(round(lx, 2), 0.8, round(lz, 2), 0.35)
            looted.append(idx)
    placed = [("Dungeon", [cols * CW / 2.0, 0, rows * RW2 / 2.0], 0, ["level"])]
    return placed, mb

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
    if pattern == "hub_spoke":
        main_mb, poi_list = payload
    else:
        main_mb = payload
    made = []
    nf = emit(main_mb, models, "layout_main", mats, assets_dir)
    made.append(("layout_main.obj", nf))
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
                   "objective": rules_a.get("generator") and "see design_rules procgen_rules",
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