#!/usr/bin/env python3
"""make_game.py - ONE COMMAND builds a COMPLETE game. The AI's workhorse.

Two modes, exactly matching how humans ask:

  1. Open-ended ("make a game"):
     python make_game.py --random
     -> picks archetype/pattern/theme/seed/name, builds EVERYTHING.

  2. Directed ("make a game about X", "like game Y"):
     python make_game.py --about "zombie mall survival at night"
     -> keyword-intent mapper chooses genre math + palette; same pipeline.
     Agent may override any part: --archetype --pattern --theme --seed --name.

Full pipeline per game (every step a shipped tool, zero hand-rolling):
   gen_<kind> (flagship dispatch or gen_archetype workhorse) -> gen_props
   (kit) -> auto-authored brief -> enrich_game -> OBJ/scene lint -> native
   proof gate (littcli validate assertions + littview render pixel proof)
   -> viewer+players deployed -> NOTES/ATTRIBUTION/LIVE_LOG ->
   Project/games.json manifest.

Kinds (--kind): soulslike | space | tabletop | platformer25d route to their
flagship generators with the user's seed/out-dir; the default archetype kind
keeps the classic gen_archetype pipeline unchanged.

Output: last stdout line is machine-readable JSON:
  {"ok":true,"game":"dead-mall","dir":"Project/dead-mall","tris":...}
"""
import argparse
import datetime
import json
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).parent
WORLDGEN = HERE / "worldgen" if (HERE / "worldgen").exists() else HERE.parent / "worldgen"
ASSETS_TOOLS = HERE if (HERE / "lint.py").exists() else HERE.parent / "assets"
REPO = ASSETS_TOOLS.parent.parent.parent   # .../template/tools/assets -> engine root
sys.path.insert(0, str(ASSETS_TOOLS))
sys.path.insert(0, str(WORLDGEN))
from lint import lint_game, solid_count  # noqa: E402
from native_proof import proof_one_game  # noqa: E402  (assets/native_proof.py)
from worldkit import Placement           # noqa: E402  (worldgen/worldkit.py)

# --------------------------------------------------------------- intent map
# Every archetype below is verified against `gen_archetype.py --list`.
INTENT_KEYWORDS = [
    # (regex on lowercase request) -> (archetype, pattern, theme, kit)
    (r"zombie|horde|swarm|infect|outbreak", "survival_horror", "arena_ring", "post_apocalypse", "survivor"),
    (r"race|racing|drive|car|kart|speed", "kart_racer", "spline_track", "modern_city_night", "platformer"),
    (r"dungeon|crypt|dwarf|mines?|cave|catacomb", "dungeon_crawler", "room_graph", "underground_caves", "souls"),
    (r"souls|boss|estus|knight|hollow|dark souls", "soulslike", "hub_spoke", "haunted_estate", "souls"),
    (r"space|alien|void|station|asteroid|galax|scifi", "walking_simulator", "hub_spoke", "space_station_core", "souls"),
    (r"plat|jump|parkour|rooftop|precision", "precision_action", "corridor_run", "cyberpunk_neon", "platformer"),
    (r"runner|endless run", "endless_runner", "spline_track", "retro_scifi", "platformer"),
    (r"metroidvania|explore.*map|backtrack", "metroidvania", "corridor_run", "deep_forest", "platformer"),
    (r"puzzle|board|chess|tile|tactic", "grid_tactics", "grid_board", "minimalist_abstract", "survivor"),
    (r"western|cowboy|wild west", "open_world_western", "arena_ring", "wild_west_frontier", "survivor"),
    (r"ocean|sea|underwater|reef|submarin", "walking_simulator", "hub_spoke", "underwater_reef", "survivor"),
    (r"island|tropical|beach|pirate", "naval_pirate", "hub_spoke", "tropical_island", "survivor"),
    (r"egypt|pyramid|tomb|pharaoh", "dungeon_crawler", "room_graph", "egyptian_desert", "souls"),
    (r"horror|ghost|haunt|scary|creepy", "psychological_horror", "corridor_run", "haunted_estate", "souls"),
    (r"candy|cute|cozy|pastel|kids?", "narrative_adventure", "grid_board", "candy_land", "survivor"),
    (r"steampunk|brass|gear|clockwork", "extraction_shooter", "corridor_run", "steampunk_brass", "survivor"),
    (r"forest|nature|druid|elf", "walking_simulator", "hub_spoke", "deep_forest", "souls"),
    (r"arctic|snow|ice|frozen", "open_world_survival", "arena_ring", "arctic_expanse", "survivor"),
    (r"military|war|soldier|trench|shoot", "tactical_shooter", "room_graph", "military_outpost", "survivor"),
    (r"rogue|dungeon crawl|permadeath", "roguelite", "grid_board", "dark_fantasy", "survivor"),
]

PLATFORMER_ARCHES = {"kart_racer", "precision_action", "endless_runner",
                     "metroidvania"}
SOULS_ARCHES = {"soulslike", "dungeon_crawler", "psychological_horror",
                "naval_pirate", "walking_simulator"}

# ITEM 7: flagship generator dispatch. --kind routes to the dedicated genre
# generators; default "archetype" keeps the classic pipeline byte-identical.
KINDS = ("soulslike", "space", "tabletop", "platformer25d", "archetype")
KIND_GEN = {"soulslike": "gen_soulslike.py", "space": "gen_space.py",
            "tabletop": "gen_tabletop.py",
            "platformer25d": "gen_platformer25d.py"}
KIND_KIT = {"soulslike": "souls", "space": "survivor",
            "tabletop": "survivor", "platformer25d": "platformer"}


def kit_for(arch):
    if arch in PLATFORMER_ARCHES:
        return "platformer"
    if arch in SOULS_ARCHES:
        return "souls"
    return "survivor"


def pick_random(rng):
    archs = ["roguelite", "bullet_hell", "soulslike", "extraction_shooter",
             "walking_simulator", "open_world_survival", "kart_racer",
             "metroidvania", "narrative_adventure", "grid_tactics"]
    pat_theme = {
        "arena_ring": ["dark_fantasy", "post_apocalypse", "arctic_expanse"],
        "corridor_run": ["haunted_estate", "steampunk_brass", "cyberpunk_neon"],
        "hub_spoke": ["haunted_estate", "deep_forest", "tropical_island",
                      "underwater_reef", "space_station_core"],
        "grid_board": ["minimalist_abstract", "candy_land", "toy_voxel_playground"],
        "spline_track": ["modern_city_night", "wild_west_frontier", "retro_scifi"],
        "room_graph": ["underground_caves", "egyptian_desert", "military_outpost"],
    }
    arch = rng.choice(archs)
    pat = rng.choice(list(pat_theme.keys()))
    theme = rng.choice(pat_theme[pat])
    return arch, pat, theme, kit_for(arch)


def map_intent(text):
    t = text.lower()
    for rx, arch, pat, theme, kit in INTENT_KEYWORDS:
        if re.search(rx, t):
            return arch, pat, theme, kit
    return None


NAME_WORDS_A = ["ember", "hollow", "iron", "ash", "neon", "grim", "last",
                "broken", "silent", "crimson", "frost", "sunken", "wired"]
NAME_WORDS_B = ["depths", "reach", "fall", "garden", "vault", "spire", "march",
                "hollow", "expanse", "bastion", "crossing", "verge"]


def auto_name(rng):
    return "%s-%s" % (rng.choice(NAME_WORDS_A), rng.choice(NAME_WORDS_B))


def scene_layout(game_dir):
    """Union bbox of walkable solids (floor/level/track/board/hub/terrain
    nodes), from their OBJ vertex bounds + node positions.

    Returns dict {min:[3], max:[3], axis: 0|2, top: y} or None."""
    import json as _json
    gdir = Path(game_dir)
    scene = _json.loads(
        (gdir / "assets/scenes/world.lscn.json").read_text(encoding="utf-8"))
    lo = [None] * 3
    hi = [None] * 3
    for node in scene["nodes"]:
        tags = set(node.get("tags", []))
        if not (tags & {"floor", "level", "track", "board", "hub", "terrain"}):
            continue
        mt = [t for t in tags if t.startswith("model:")]
        if not mt:
            continue
        obj = gdir / "assets/models" / (mt[0][6:] + ".obj")
        if not obj.exists():
            continue
        bmin = [1e9] * 3
        bmax = [-1e9] * 3
        nv = 0
        for ln in obj.read_text(encoding="utf-8").splitlines():
            p = ln.split()
            if p and p[0] == "v":
                nv += 1
                for i in range(3):
                    v = float(p[i + 1])
                    if v < bmin[i]:
                        bmin[i] = v
                    if v > bmax[i]:
                        bmax[i] = v
        if not nv:
            continue
        pos = node.get("position", [0, 0, 0])
        for i in range(3):
            a, b = bmin[i] + pos[i], bmax[i] + pos[i]
            lo[i] = a if lo[i] is None else min(lo[i], a)
            hi[i] = b if hi[i] is None else max(hi[i], b)
    if lo[0] is None or hi[0] is None:
        return None
    span_x = hi[0] - lo[0]
    span_z = hi[2] - lo[2]
    return {"min": lo, "max": hi,
            "axis": 0 if span_x >= span_z else 2,
            "top": hi[1]}


def place_on(layout, frac, lift=1.2):
    """Point standing ON the solid span at fraction along its long axis."""
    a = layout["axis"]
    p = [layout["min"][0] + (layout["max"][0] - layout["min"][0]) * 0.5,
         layout["top"] + lift,
         layout["min"][2] + (layout["max"][2] - layout["min"][2]) * 0.5]
    p[a] = layout["min"][a] + (layout["max"][a] - layout["min"][a]) * frac
    return [round(p[0], 2), round(p[1], 2), round(p[2], 2)]


# ITEM 9: collision-aware scatter of make_game's own extras.
EXTRA_HALF_M = 0.6   # occupancy box half-extent per scattered extra
EXTRA_TRIES = 10     # bounded deterministic re-rolls before dropping


class ExtraPlacer:
    """Keeps story-item/roster extra nodes from stacking on each other or on
    brief anchors, via the worldkit.Placement AABB registry.

    The derived walkable layout is registered as GROUND; every pre-existing
    anchor (spawn/checkpoints/authored brief nodes) is registered SOLID. A
    candidate that conflicts re-rolls along the spine a bounded number of
    seeded times, then is dropped instead of overlapping. Same seed -> same
    draw sequence -> byte-identical worlds."""

    def __init__(self, layout, seed, anchors=()):
        self.layout = layout
        self.rng = random.Random("extras/%d" % seed)
        self.dropped = 0
        self.reg = Placement()
        if not layout:
            return
        lo, hi = layout["min"], layout["max"]
        self.reg.insert("<ground>", (lo[0], lo[2]), (hi[0], hi[2]),
                        top=layout["top"], walkable=True)
        for i, p in enumerate(anchors):
            if not p:
                continue
            self.reg.insert("anchor:%02d" % i,
                            (p[0] - EXTRA_HALF_M, p[2] - EXTRA_HALF_M),
                            (p[0] + EXTRA_HALF_M, p[2] + EXTRA_HALF_M))

    def place(self, name, frac, lift=1.2, tries=EXTRA_TRIES):
        """Collision-free spine point for `name`, or None to drop it."""
        if self.layout is None:
            return None
        lay = self.layout
        cross = 0 if lay["axis"] == 2 else 2          # the deck's short axis
        c_lo = lay["min"][cross] + EXTRA_HALF_M
        c_hi = lay["max"][cross] - EXTRA_HALF_M
        f = frac
        blockers = "?"
        for attempt in range(tries):
            p = place_on(lay, min(max(f, 0.02), 0.98), lift=lift)
            if attempt and c_hi > c_lo:               # widen across the deck,
                p[cross] = round(                     # never off the walkable
                    min(max(p[cross] + self.rng.uniform(-3.0, 3.0) * attempt,
                            c_lo), c_hi), 2)
            lo = (p[0] - EXTRA_HALF_M, p[2] - EXTRA_HALF_M)
            hi = (p[0] + EXTRA_HALF_M, p[2] + EXTRA_HALF_M)
            if self.reg.insert(name, lo, hi):
                return p
            blockers = ", ".join(list(self.reg.conflicts(lo, hi))[:3])
            f = frac + self.rng.uniform(-0.09, 0.09) * (attempt + 1)
        self.dropped += 1
        print("[make] placement: dropped %r after %d spots (blocked by: %s)"
              % (name, tries, blockers))
        return None


def brief_for(kit, theme, name, seed, prompt_text, layout=None):
    """Auto-author a feature-rich gameplay brief from kit templates."""
    rng = random.Random(seed)
    flavor = theme.replace("_", " ")
    # geometry-aware anchors: everything sits ON the actual walkable span
    if layout:
        origin = [round((layout["min"][0] + layout["max"][0]) * 0.5, 2),
                  round((layout["min"][2] + layout["max"][2]) * 0.5, 2)]
    else:
        origin = [0.0, 0.0]

    def ox(p):  # shift a constant [x,y,z] into the real layout
        return [round(p[0] + origin[0], 2), p[1], round(p[2] + origin[1], 2)]

    if kit == "platformer":
        if layout:
            spawn_p = place_on(layout, 0.05)
            cps = [place_on(layout, f) for f in (0.28, 0.52, 0.76)]
            drone_a = place_on(layout, 0.38, lift=4.6)
            drone_b = place_on(layout, 0.62, lift=5.0)
            gem_hi = place_on(layout, 0.14, lift=3.6)
            spikes = place_on(layout, 0.45)
            banner = place_on(layout, 0.97, lift=0.2)
            trail = [place_on(layout, f, lift=1.6) for f in
                     (0.20, 0.33, 0.58, 0.70, 0.84)]
        else:
            spawn_p = [1.5, 0.0, 0.0]
            cps = [[18, 0, 0], [36, 0, 0], [52, 0, 0], [68, 0, 0]]
            drone_a = [27, 4.6, 0]
            drone_b = [43, 5.0, 0]
            gem_hi = [10.5, 3.6, 0]
            spikes = [39.5, 0, 0]
            banner = [69, 0.2, 0]
            trail = []
        nodes = [
            {"name": "Drone_A", "pos": drone_a, "tags": ["enemy", "hazard", "model:drone"]},
            {"name": "Drone_B", "pos": drone_b, "tags": ["enemy", "hazard", "model:drone"]},
            {"name": "Gem_High", "pos": gem_hi, "tags": ["pickup", "score", "model:gem"]},
            {"name": "Spikes_Mid", "pos": spikes, "tags": ["hazard", "model:spike"]},
        ]
        for i, t in enumerate(trail):
            nodes.append({"name": "Coin_%02d" % (i + 1), "pos": t,
                          "tags": ["pickup", "score", "model:coin"]})
        if layout:
            # visible finish line: the run ends when you reach the banner
            nodes.append({"name": "Win_Banner", "pos": banner,
                          "tags": ["goal", "poi", "model:banner"]})
        brief = {
            "objective": "%s: ride the %s ridge, slip the drones, plant your run at the summit banner" % (name, flavor),
            "side_objectives": ["No-death run", "Every coin on the trail",
                                "Reach the banner under par"],
            "physics": {"gravity": 30, "jump_velocity": 12, "run_speed": 8,
                        "coyote_time_s": 0.12},
            "lives": 3,
            "enemy_aggro_m": 4.5, "corpse_run": False,
            "scoring": {"coins": 25},
            "roster": [{"name": "Sentinel Drone",
                        "behavior": "patrols its platform; dive on proximity"}],
            "spawn": spawn_p,
            "checkpoints": cps,
            "nodes": nodes,
            "zones": [{"name": flavor.title() + " Ridge", "pos": ox([36, 0, 0]), "radius": 42,
                       "tags": ["music:zone_" + kit]}],
        }
    elif kit == "souls":
        if layout:
            spawn_p = place_on(layout, 0.18)
            cp_a = place_on(layout, 0.32)
            cp_b = place_on(layout, 0.72)
            shrine = place_on(layout, 0.22)
            knight = place_on(layout, 0.66)
            stalker = place_on(layout, 0.5)
            estus = place_on(layout, 0.82)
            gate = place_on(layout, 0.93, lift=0.2)
        else:
            spawn_p = [0.0, 0.0, 3.0]
            cp_a = [0, 0, 6.5]
            cp_b = [-18, 0, 13.0]
            shrine = [0, 0, 6.5]
            knight = [23, 0, 0]
            stalker = [16, 0, 16]
            estus = [27.5, 0, 2.0]
            gate = [40, 0.2, 0]
        brief = {
            "objective": "%s: kindle the bonfires of the %s court and force the fog gate at the far banner" % (name, flavor),
            "side_objectives": ["Kindle every bonfire in one run",
                                "Bank 800 points before the gate",
                                "Never die twice to the same knight"],
            "physics": {"gravity": 26, "run_speed": 5.5, "coyote_time_s": 0.1},
            "enemy_aggro_m": 9.0, "corpse_run": True,
            "scoring": {"coins": 25},
            "roster": [
                {"name": "Hollow Knight", "behavior": "guards the mid-court shrine; lunge inside 3 m"},
                {"name": "Garden Stalker", "behavior": "patrols the long road; drops chase at 12 m"},
            ],
            "spawn": spawn_p,
            "checkpoints": [cp_a, cp_b],
            "nodes": [
                {"name": "Bonfire_Plaza", "pos": shrine, "tags": ["checkpoint", "poi", "model:bonfire"]},
                {"name": "Knight_Sun", "pos": knight, "tags": ["enemy", "model:knight"]},
                {"name": "Stalker_Rose", "pos": stalker, "tags": ["enemy", "model:stalker"]},
                {"name": "Estus_Hidden", "pos": estus, "tags": ["pickup", "score", "model:estus_flask"]},
                {"name": "Fog_Gate_Banner", "pos": gate,
                 "tags": ["goal", "poi", "model:banner"]},
            ],
            "zones": [{"name": flavor.title() + " Court", "pos": ox([0, 0, 0]),
                       "radius": 26, "tags": ["music:hollow_wind"]}],
        }
    else:  # survivor
        if layout:
            spawn_p = place_on(layout, 0.5)
            wraith_n = ox([0, 0, -8])
            wraith_e = ox([8, 0, 0])
            brute_g = ox([0, 0, 11.5])
            gem_ne = ox([3.5, 0, 3.5])
            heart_w = ox([-3, 0, 2])
            zone_pos = ox([0, 0, 0])
            cps = [ox([9.9, 0, 3.1]), ox([-7.4, 0, -6.9])]
        else:
            spawn_p = [0.0, 0.0, 4.5]
            wraith_n = [0, 0, -8]
            wraith_e = [8, 0, 0]
            brute_g = [0, 0, 11.5]
            gem_ne = [3.5, 0, 3.5]
            heart_w = [-3, 0, 2]
            zone_pos = [0, 0, 0]
            cps = [[9.9, 0, 3.1], [-7.4, 0, -6.9]]
        brief = {
            "objective": "%s: hold the %s arena against the Legion - six waves, then the sky opens" % (name, flavor),
            "side_objectives": ["Light every brazier before wave three",
                                "Reach 600 points without a death"],
            "physics": {"gravity": 24, "run_speed": 6.5, "coyote_time_s": 0.1},
            "lives": 3,
            "score_goal": 600,
            "enemy_aggro_m": 8.5, "corpse_run": False,
            "scoring": {"coins": 25},
            "waves": [{"at_score": s * 150, "note": "wave %d" % (s + 1)}
                      for s in range(6)],
            "roster": [
                {"name": "Ember Wraith", "behavior": "fast ring-dodger, dives when aggroed"},
                {"name": "Ash Brute", "behavior": "slow tank guarding gates"},
            ],
            "spawn": spawn_p,
            "checkpoints": cps,
            "nodes": [
                {"name": "Wraith_N", "pos": wraith_n, "tags": ["enemy", "model:wraith"]},
                {"name": "Wraith_E", "pos": wraith_e, "tags": ["enemy", "model:wraith"]},
                {"name": "Brute_Gate", "pos": brute_g, "tags": ["enemy", "model:brute"]},
                {"name": "Gem_NE", "pos": gem_ne, "tags": ["pickup", "score", "model:gem"]},
                {"name": "Heart_W", "pos": heart_w, "tags": ["pickup", "score", "model:heart"]},
            ],
            "zones": [{"name": flavor.title() + " Arena", "pos": zone_pos,
                       "radius": 15, "tags": ["music:ember_dread"]}],
        }
    brief["_prompt"] = prompt_text or ("a %s game" % flavor)
    return brief


def run(*args):
    r = subprocess_run([sys.executable, *args])
    if r.returncode != 0:
        raise RuntimeError("%s failed:\n%s\n%s"
                           % (" ".join(map(str, args[:2])),
                              r.stdout[-500:], r.stderr[-500:]))
    return r.stdout


def subprocess_run(args):
    import subprocess
    return subprocess.run(args, capture_output=True, text=True)


def _json_tail(text):
    """Last {...} line of a tool's stdout (littcli validate reports JSON)."""
    for ln in reversed((text or "").strip().splitlines()):
        ln = ln.strip()
        if ln.startswith("{"):
            try:
                return json.loads(ln)
            except Exception:
                continue
    return {}


def littcli_validate(game_dir, cli):
    """Native C validator --frames 120. Same contract as before: prints the
    report and exits 1 on nonzero exit code; returns the parsed JSON."""
    vr = subprocess.run([str(cli), "validate", str(game_dir),
                         "--frames", "120"],
                        capture_output=True, text=True, timeout=60)
    print(vr.stdout.strip())
    if vr.returncode != 0:
        print(json.dumps({"ok": False, "game": Path(game_dir).name,
                          "validator": "littcli"}, indent=2))
        sys.exit(1)
    return _json_tail(vr.stdout)


# ITEM 8: native proof gate thresholds (match template/tools/assets/native_proof.py)
MIN_FILL_PCT = 1.5
MIN_COLORS = 8


def native_proof_gate(game_dir, name, cli, view, sim_json=None):
    """Hard proof the freshly built game really plays and renders.

    Render + pixel gating is delegated to native_proof.proof_one_game
    (single source of truth shared with template/tools/assets/
    native_proof.py - audit 5.1). make_game keeps its stricter sim
    assertions on top: validate ok:true AND interactives > 0 AND
    missing == 0.

    Every failed assertion is printed by name; any failure exits nonzero.
    Returns a small stats dict for the final machine-readable JSON line."""
    gdir = Path(game_dir)
    problems = []
    js = sim_json

    if js is None:
        if not Path(cli).exists():
            problems.append("littcli binary present at %s" % cli)
        else:
            vr = subprocess.run([str(cli), "validate", str(gdir),
                                 "--frames", "120"],
                                capture_output=True, text=True, timeout=60)
            print(vr.stdout.strip())
            js = _json_tail(vr.stdout)
            if vr.returncode != 0:
                problems.append("littcli validate exit 0 (got %s)"
                                % vr.returncode)

    rec = proof_one_game(gdir, cli, view, MIN_FILL_PCT, MIN_COLORS, sim=js)
    if js is not None:   # same guard as before: sim asserts need a simulator
        if rec["sim"] != "ok":
            problems.append("validate ok:true (got %r)" % (js.get("ok"),))
        n_inter = rec.get("interactives")
        if not isinstance(n_inter, int) or n_inter <= 0:
            problems.append("interactives > 0 (got %r)" % (n_inter,))
        if rec.get("missing") != 0:
            problems.append("missing == 0 (got %r)" % (rec.get("missing"),))
    # proof_one_game's own render/binary findings; its generic sim line is
    # skipped because make_game already reported it with the concrete value
    problems.extend(p for p in rec.get("problems", [])
                    if p != "validate ok:true")

    if problems:
        for p in problems:
            print("[make] native-proof FAIL: expected %s" % p)
        print(json.dumps({"ok": False, "game": name,
                          "stage": "native-proof", "failed": problems},
                         indent=2))
        sys.exit(1)
    print("[make] native-proof: PASS | ok=true solids=%s interactives=%s "
          "missing=%s | fill=%.2f%% colors=%d (gates: fill>=%.1f%% "
          "colors>=%d)"
          % (js.get("solids"), rec.get("interactives"), rec.get("missing"),
             rec.get("fill_pct", 0.0), rec.get("colors", 0),
             MIN_FILL_PCT, MIN_COLORS))
    return {"fill": round(rec.get("fill_pct", 0.0), 2),
            "colors": rec.get("colors", 0),
            "interactives": rec.get("interactives"),
            "missing": rec.get("missing")}


def deploy_runtime(game_dir, port_seed):
    g = Path(game_dir)
    (g / "viewer").mkdir(exist_ok=True)
    rt = REPO / "template/tools/runtime"
    ex = REPO / "Project/example-village"
    # HTML/browser stack is PHASED OUT entirely. Two native paths remain:
    #  - ENGINE.bat/.sh  -> the engine's Vulkan player (`litt play <dir>`):
    #    FSR, path tracing, every render feature. THE way to play.
    #  - VIEW.bat        -> littview, the C++ orbit viewer from native/:
    #    zero-dependency visual check of the built world.
    shutil.copy(ex / "play_native.py", g / "")
    # native C++ orbit viewer launcher (littview window)
    view = ("@echo off\nrem %s - native C++ viewer (littview)\n"
            "cd /d \"%%~dp0\"\n"
            "start \"littview\" \"..\\..\\native\\bin\\littview.exe\" window .\n"
            % g.name)
    (g / "VIEW.bat").write_text(view, encoding="ascii")
    nbat = ("@echo off\nrem %s headless validation (CI smoke)\ncd /d \"%%~dp0\"\n"
            "python play_native.py --frames 60 --dummy\nif errorlevel 1 pause" % g.name)
    (g / "VALIDATE.bat").write_text(nbat, encoding="ascii")
    # cross-platform native launchers (Windows + Linux/macOS)
    from gen_launchers import BAT as _LBAT, SH as _LSH
    (g / "ENGINE.bat").write_text(
        _LBAT.replace("\\\\", "\\"), encoding="ascii", newline="\r\n")
    sh_path = g / "ENGINE.sh"
    sh_path.write_text(_LSH, encoding="utf-8", newline="\n")
    sh_path.chmod(0o755)
    del rt
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--random", action="store_true",
                    help="build a surprise complete game")
    ap.add_argument("--about", default=None,
                    help='e.g. "a game about zombie mall survival"')
    ap.add_argument("--name", default=None)
    ap.add_argument("--out-dir", default=None, help="default Project/<name>")
    ap.add_argument("--archetype"); ap.add_argument("--pattern")
    ap.add_argument("--theme"); ap.add_argument("--seed", type=int)
    ap.add_argument("--kit", choices=["survivor", "platformer", "souls"])
    ap.add_argument("--time-of-day", default=None,
                    help="dawn|noon|dusk|night (passed to gen_archetype)")
    ap.add_argument("--weather", default=None,
                    help="clear|rain|snow (passed to gen_archetype)")
    ap.add_argument("--skip-validate", action="store_true")
    ap.add_argument("--kind", default="archetype", choices=list(KINDS),
                    help="generator family: flagship soulslike/space/"
                         "tabletop/platformer25d generators, or the "
                         "archetype workhorse (default = classic pipeline)")
    ap.add_argument("--skip-native-proof", action="store_true",
                    help="escape hatch: skip the validate+render proof gate")
    ap.add_argument("--scale", default=None, choices=["small", "medium", "full"],
                    help="story/content scope: small demo, medium game, "
                         "full RPG (acts/items/roster size)")
    a = ap.parse_args()

    if (not a.random and not a.about and not a.archetype
            and a.kind == "archetype"):
        ap.error('pass --random or --about "description" (or --kind)')

    # scale from explicit flag, else read it out of the human's wording
    scale = a.scale
    if scale is None and a.about:
        t = a.about.lower()
        if any(w in t for w in ("full", "big", "huge", "epic", "rpg",
                                "long", "entire", "whole")):
            scale = "full"
        elif any(w in t for w in ("small", "quick", "short", "tiny",
                                  "minimal", "demo")):
            scale = "small"
    scale = scale or "medium"

    seed = a.seed if a.seed is not None else random.randrange(10_000)
    rng = random.Random(seed)

    intent = map_intent(a.about) if a.about else None
    if a.random:
        arch, pat, theme, kit = pick_random(rng)
    elif intent:
        arch, pat_raw, theme, kit = intent
        pat = {"rooms_pattern": "room_graph"}.get(pat_raw, pat_raw)
    else:
        arch, pat, theme, kit = pick_random(rng)
    arch = a.archetype or arch
    pat = a.pattern or pat
    theme = a.theme or theme
    kind = a.kind
    kit = a.kit or (KIND_KIT[kind] if kind != "archetype" else kit)
    # 2D5 camera archetypes move on one axis only - curved spline decks drop
    # the player off the world. Force a straight pattern for side-view kits.
    if kit == "platformer" and pat in {"spline_track", "room_graph"}:
        print("[make] %s + %s would break side-view movement -> corridor_run"
              % (arch, pat))
        pat = "corridor_run"
    name = a.name or auto_name(rng)
    out = Path(a.out_dir) if a.out_dir else REPO / "Project" / name
    if out.exists():
        name = name + "-%d" % (seed % 1000)
        out = Path(a.out_dir) if a.out_dir else REPO / "Project" / name
    print("[make] building '%s' | kind=%s %s/%s/%s kit=%s seed=%d -> %s"
          % (name, kind, arch, pat, theme, kit, seed, out))

    # ---- geometry: kind-dispatched generators ----------------------------
    # ITEM 7: flagship kinds run their dedicated generator with the user's
    # seed/out-dir; default archetype path is exactly today's behavior.
    prompt = a.about or ("random %s build" % kind)
    if kind != "archetype":
        common = ["--out-dir", str(out), "--seed", str(seed),
                  "--agent", "ai-agent", "--prompt", prompt]
        run(WORLDGEN / KIND_GEN[kind], *common)
    elif arch == "platformer25d":
        # explicit opt-in to the true side-scroller generator
        run(WORLDGEN / "gen_platformer25d.py", "--out-dir", str(out),
            "--agent", "ai-agent", "--prompt", a.about or "random")
    else:
        cmd = ["--archetype", arch, "--pattern", pat, "--theme", theme,
               "--seed", str(seed), "--name", name, "--out-dir", str(out),
               "--prompt", a.about or "random"]
        if a.time_of_day:
            cmd += ["--time-of-day", a.time_of_day]
        if a.weather:
            cmd += ["--weather", a.weather]
        run(WORLDGEN / "gen_archetype.py", *cmd)
    run(WORLDGEN / "gen_props.py", "--game-dir", str(out), "--kit", kit)

    # derive gameplay anchors from the REAL generated geometry
    layout = scene_layout(out)

    brief = brief_for(kit, theme, name, seed, a.about, layout)

    # ---- narrative layer (story acts, items, roster) ---------------------
    run(WORLDGEN / "gen_story.py", "--about", a.about or name,
        "--game-dir", str(out), "--archetype", arch,
        "--scale", scale, "--seed", str(seed))
    try:
        items = json.loads((out / "story/items.json").read_text(
            encoding="utf-8")).get("items", [])
        roster = json.loads((out / "story/roster.json").read_text(
            encoding="utf-8")).get("roster", [])
    except Exception as exc:  # story is enhancement, never fatal
        print("[make] story merge skipped: %s" % exc)
        items, roster = [], []

    def _model_for_rarity(rarity):
        return {"legendary": "objective", "rare": "token"}.get(rarity, "coin")

    ENEMY_MODEL = {"souls": ("wraith", "knight", "brute"),
                   "platformer": ("drone", "stalker", "brute")}
    emook, eelite, eboss = ENEMY_MODEL.get(kit, ("drone", "stalker", "brute"))
    # only reference meshes this game's prop kit actually shipped
    models_dir = out / "assets/models"
    have = ({p.stem for p in models_dir.glob("*.obj")}
            if models_dir.is_dir() else set())

    def have_any(*names):
        return next((n for n in names if n in have), None)

    pickup_for = {
        "coin": have_any("coin"),
        "token": have_any("token", "gem", "coin"),
        "objective": have_any("objective", "estus_flask", "gem", "coin"),
    }
    emook = have_any("drone", "stalker", "wraith", "brute")
    eelite = have_any("stalker", "knight", "brute", emook)
    eboss = have_any("brute", "knight", "wraith", emook)

    # ITEM 9: collision-aware scatter of this builder's OWN extras (story
    # items + roster enemies). Authored brief anchors are registered first;
    # conflicting candidates re-roll deterministically, then get dropped.
    anchors = ([brief["spawn"]] if brief.get("spawn") else [])
    anchors += list(brief.get("checkpoints") or [])
    anchors += [n["pos"] for n in brief.get("nodes", []) if n.get("pos")]
    placer = ExtraPlacer(layout, seed, anchors)

    existing = {n.get("name") for n in brief.get("nodes", [])}
    for i, it in enumerate(items):
        nm = "Item_%02d_%s" % (i, "".join(ch for ch in it["name"].title()
                                          if ch.isalnum())[:18])
        mdl = pickup_for[_model_for_rarity(it.get("rarity"))]
        if nm in existing or not mdl:
            continue
        frac = 0.14 + (0.72 * (i + 1)) / max(1, len(items))
        ipos = placer.place(nm, min(frac, 0.9))
        if ipos is None:
            continue
        brief.setdefault("nodes", []).append({
            "name": nm, "pos": ipos,
            "tags": ["scoring", "model:%s" % mdl],
            "poi": "%s (%s)" % (it["name"], it["rarity"]),
        })
        existing.add(nm)
    bosses = [r for r in roster if r["role"] == "boss"]
    others = [r for r in roster if r["role"] != "boss"]
    for i, r in enumerate(bosses):
        nm = "Boss_%d_%s" % (i, "".join(ch for ch in r["name"]
                                        if ch.isalnum())[:16])
        if nm not in existing and eboss:
            bpos = placer.place(nm, 0.35 + 0.18 * i)
            if bpos is None:
                continue
            brief.setdefault("nodes", []).append({
                "name": nm,
                "pos": bpos,
                "tags": ["enemy", "hazard", "model:%s" % eboss],
                "poi": r["description"],
            })
            existing.add(nm)
    for i, r in enumerate(others):
        role = r["role"]
        # engine reads combat tier from the name prefix
        pfx = "Elite_" if role == "elite" else "Mook_"
        nm = "%s%02d_%s" % (pfx, i,
                            "".join(ch for ch in r["name"]
                                    if ch.isalnum())[:16])
        mdl = eelite if role == "elite" else emook
        if nm in existing or not mdl:
            continue
        frac = 0.22 + (0.62 * (i + 1)) / max(1, len(others))
        opos = placer.place(nm, min(frac, 0.86))
        if opos is None:
            continue
        brief.setdefault("nodes", []).append({
            "name": nm,
            "pos": opos,
            "tags": ["enemy", "model:%s" % mdl],
            "poi": r["description"],
        })
        existing.add(nm)
    print("[make] story layer: %d items -> pickups, %d roster -> enemies "
          "(collision placement dropped %d)"
          % (len(items), len(roster), placer.dropped))

    brief_path = out / "brief.json"
    brief_path.write_text(json.dumps(brief, indent=2), encoding="utf-8")
    run(WORLDGEN / "enrich_game.py", "--game-dir", str(out),
        "--brief", str(brief_path), "--seed", str(seed))

    # ---- validate ---------------------------------------------------------
    report = lint_game(out)
    scene = out / "assets/scenes/world.lscn.json"
    solids = solid_count(scene)
    ok = not report["problems"] and not report["dangling_refs"]
    if not ok:
        print(json.dumps({"ok": False, "game": name, "dir": str(out),
                          "lint": report}, indent=2))
        sys.exit(1)

    tris = "?"
    # native C validator (Stage-1 littcore) when built; Python fallback else
    cli = REPO / "native" / "bin" / "littcli.exe"
    if not cli.exists():
        cli = REPO / "native" / "bin" / "littcli"
    view = REPO / "native" / "bin" / "littview.exe"
    if not view.exists():
        view = REPO / "native" / "bin" / "littview"
    sim_json = None
    if not a.skip_validate:
        if cli.exists():
            sim_json = littcli_validate(out, cli)
        else:
            run(out / "play_native.py", "--project", str(out),
                "--frames", "30", "--dummy")

    # ITEM 8: native proof gate (validate assertions + rendered-pixel proof)
    proof = None
    if a.skip_native_proof:
        print("[make] native-proof: SKIPPED (--skip-native-proof)")
    else:
        proof = native_proof_gate(out, name, cli, view, sim_json)
    deploy_runtime(out, seed)

    proof_note = ("fill=%.2f%% colors=%d interactives=%s missing=%s"
                  % (proof["fill"], proof["colors"], proof["interactives"],
                     proof["missing"])) if proof else "skipped"

    # NOTES + ATTRIBUTION + manifest ---------------------------------------
    (out / "NOTES.md").write_text(
        "# NOTES - %s\n\n%s\n\n- built by: make_game.py (%s mode)\n"
        "- kind=%s archetype=%s pattern=%s theme=%s kit=%s seed=%d scale=%s\n"
        "- lint: clean | solids nodes: %d | native proof: %s\n"
        "- play: ENGINE.bat/.sh (Vulkan player) | VIEW.bat (C++ viewer)\n"
        "- story: story/story.md (+items.json, +roster.json)\n"
        % (name, (a.about or "random pick"), "about" if a.about else "random",
           kind, arch, pat, theme, kit, seed, scale, solids, proof_note),
        encoding="utf-8")
    (out / "ATTRIBUTION.md").write_text(
        "# ATTRIBUTION - %s\n\nAll assets procedurally generated by Litt "
        "worldgen tools. No third-party content.\n" % name, encoding="utf-8")

    manifest_p = REPO / "Project/games.json"
    # Orchestrators (refine_game.py, world_forge.py) set LITT_NO_MANIFEST=1
    # for their scratch builds; only the final deploy registers.
    if os.environ.get("LITT_NO_MANIFEST") != "1":
        try:
            manifest = json.loads(manifest_p.read_text(encoding="utf-8"))
        except Exception:
            manifest = {"games": []}
        manifest["games"] = [g for g in manifest["games"] if g["name"] != name]
        try:
            rel_dir = str(out.relative_to(REPO))
        except ValueError:
            rel_dir = str(out)
        manifest["games"].append({
            "name": name, "dir": rel_dir,
            "archetype": arch, "pattern": pat, "theme": theme, "kit": kit,
            "seed": seed, "about": a.about or "", "built":
                datetime.datetime.now().isoformat(timespec="seconds")})
        manifest_p.write_text(json.dumps(manifest, indent=2),
                              encoding="utf-8")

    final = {"ok": True, "game": name, "dir": str(out),
             "objs": report["objs"], "solids_nodes": solids,
             "play": "ENGINE.bat/.sh", "view": "VIEW.bat"}
    if proof:
        final["native_proof"] = proof
    print(json.dumps(final))


if __name__ == "__main__":
    main()
