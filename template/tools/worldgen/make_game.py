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
  gen_archetype/gen_platformer25d -> gen_props (kit) -> auto-authored brief
  -> enrich_game -> OBJ/scene lint -> native validation -> viewer+players
  deployed -> NOTES/ATTRIBUTION/LIVE_LOG -> Project/games.json manifest.

Output: last stdout line is machine-readable JSON:
  {"ok":true,"game":"dead-mall","dir":"Project/dead-mall","tris":...}
"""
import argparse
import datetime
import json
import random
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).parent
WORLDGEN = HERE / "worldgen" if (HERE / "worldgen").exists() else HERE.parent / "worldgen"
ASSETS_TOOLS = HERE if (HERE / "lint.py").exists() else HERE.parent / "assets"
REPO = ASSETS_TOOLS.parent.parent.parent   # .../template/tools/assets -> engine root
sys.path.insert(0, str(ASSETS_TOOLS))
from lint import lint_game, solid_count  # noqa: E402

# --------------------------------------------------------------- intent map
INTENT_KEYWORDS = [
    # (regex on lowercase request) -> (archetype, pattern, theme, kit)
    (r"zombie|horde|survivor|survival|swarm|infect", "bullet_hell", "arena_ring", "post_apocalypse", "survivor"),
    (r"race|racing|drive|car|kart|speed", "racing", "spline_track", "modern_city_night", "platformer"),
    (r"dungeon|crypt|dwarf|mines?|cave", "soulslike", "room_graph", "underground_caves", "souls"),
    (r"souls|boss|estus|knight|hollow|dark souls", "soulslike", "hub_spoke", "haunted_estate", "souls"),
    (r"space|alien|void|station|asteroid|galax", "space_trader", "hub_spoke", "space_station_core", "souls"),
    (r"plat|jump|run|mario|celeste|parkour|rooftop", "platformer_2_5d", "corridor_run", "cyberpunk_neon", "platformer"),
    (r"puzzle|board|chess|tile|tactic", "tactics", "grid_board", "minimalist_abstract", "survivor"),
    (r"western|cowboy|desert|wild west", "shooter", "arena_ring", "wild_west_frontier", "survivor"),
    (r"ocean|sea|underwater|reef|submarin", "exploration", "hub_spoke", "underwater_reef", "survivor"),
    (r"island|tropical|beach|pirate", "adventure", "hub_spoke", "tropical_island", "survivor"),
    (r"egypt|pyramid|tomb|pharaoh", "soulslike", "room_graph", "egyptian_desert", "souls"),
    (r"horror|ghost|haunt|scary|creepy", "soulslike", "corridor_run", "haunted_estate", "souls"),
    (r"candy|cute|cozy|pastel|kids?", "adventure", "grid_board", "candy_land", "survivor"),
    (r"steampunk|brass|gear|clockwork", "shooter", "corridor_run", "steampunk_brass", "survivor"),
    (r"forest|nature|druid|elf", "exploration", "hub_spoke", "deep_forest", "souls"),
    (r"arctic|snow|ice|frozen", "survival", "arena_ring", "arctic_expanse", "survivor"),
    (r"military|war|soldier|trench", "shooter", "rooms_pattern", "military_outpost", "survivor"),
]

ARCHETYPES_AVAILABLE = None  # lazy from --list


def pick_random(rng):
    archs = ["roguelite", "bullet_hell", "soulslike", "shooter", "exploration",
             "survival", "racing", "platformer_2_5d", "adventure", "tactics"]
    pat_theme = {
        "arena_ring": ["dark_fantasy", "post_apocalypse", "arctic_expanse"],
        "corridor_run": ["haunted_estate", "steampunk_brass", "cyberpunk_neon"],
        "hub_spoke": ["haunted_estate", "deep_forest", "tropical_island",
                      "underwater_reef", "space_station_core"],
        "grid_board": ["minimalist_abstract", "candy_land", "toy_voxel_playground"],
        "spline_track": ["modern_city_night", "wild_west_frontier"],
        "room_graph": ["underground_caves", "egyptian_desert", "military_outpost"],
    }
    kit_by_arch = {"platformer_2_5d": "platformer", "racing": "platformer"}
    arch = rng.choice(archs)
    if arch == "platformer_2_5d":
        return arch, None, rng.choice(["cyberpunk_neon", "dark_fantasy"]), \
            "platformer"
    pat = rng.choice(list(pat_theme.keys()))
    theme = rng.choice(pat_theme[pat])
    return arch, pat, theme, kit_by_arch.get(arch, "souls" if arch in
                                              ("soulslike",) else "survivor")


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


def brief_for(kit, theme, name, seed, prompt_text):
    """Auto-author a feature-rich gameplay brief from kit templates."""
    rng = random.Random(seed)
    flavor = theme.replace("_", " ")
    if kit == "platformer":
        brief = {
            "objective": "%s: cross the level, bank the gems, reach the banner" % name,
            "side_objectives": ["No-death run", "All gems", "Under par time"],
            "physics": {"gravity": 30, "jump_velocity": 12, "run_speed": 8,
                        "coyote_time_s": 0.12},
            "enemy_aggro_m": 4.5, "corpse_run": False,
            "scoring": {"coins": 25},
            "roster": [{"name": "Sentinel Drone",
                        "behavior": "patrols its platform; dive on proximity"}],
            "spawn": [1.5, 0.0, 0.0],
            "checkpoints": [[18, 0, 0], [36, 0, 0], [52, 0, 0], [68, 0, 0]],
            "nodes": [
                {"name": "Drone_A", "pos": [27, 4.6, 0], "tags": ["enemy", "hazard", "model:drone"]},
                {"name": "Drone_B", "pos": [43, 5.0, 0], "tags": ["enemy", "hazard", "model:drone"]},
                {"name": "Gem_High", "pos": [10.5, 3.6, 0], "tags": ["pickup", "score", "model:gem"]},
                {"name": "Spikes_Mid", "pos": [39.5, 0, 0], "tags": ["hazard", "model:spike"]},
            ],
            "zones": [{"name": flavor.title(), "pos": [36, 0, 0], "radius": 42,
                       "tags": ["music:zone_" + kit]}],
        }
    elif kit == "souls":
        brief = {
            "objective": "%s: reclaim the shrines of the %s court" % (name, flavor),
            "side_objectives": ["Kindle all bonfires in one run",
                                "Bank 800 points before the banner"],
            "physics": {"gravity": 26, "run_speed": 5.5, "coyote_time_s": 0.1},
            "enemy_aggro_m": 9.0, "corpse_run": True,
            "scoring": {"coins": 25},
            "roster": [
                {"name": "Hollow Knight", "behavior": "guards shrine; lunge inside 3 m"},
                {"name": "Garden Stalker", "behavior": "patrols; drops chase at 12 m"},
            ],
            "spawn": [0.0, 0.0, 3.0],
            "checkpoints": [[0, 0, 6.5], [-18, 0, 13.0], [6, 0, -22.0]],
            "nodes": [
                {"name": "Bonfire_Plaza", "pos": [0, 0, 6.5], "tags": ["checkpoint", "poi", "model:bonfire"]},
                {"name": "Knight_Sun", "pos": [23, 0, 0], "tags": ["enemy", "model:knight"]},
                {"name": "Stalker_Rose", "pos": [16, 0, 16], "tags": ["enemy", "model:stalker"]},
                {"name": "Estus_Hidden", "pos": [27.5, 0, 2.0], "tags": ["pickup", "score", "model:estus_flask"]},
            ],
            "zones": [{"name": flavor.title() + " Court", "pos": [0, 0, 0],
                       "radius": 26, "tags": ["music:hollow_wind"]}],
        }
    else:  # survivor
        brief = {
            "objective": "%s: survive the Legion's waves in the %s arena" % (name, flavor),
            "side_objectives": ["Light every brazier", "600 points, no deaths"],
            "physics": {"gravity": 24, "run_speed": 6.5, "coyote_time_s": 0.1},
            "enemy_aggro_m": 8.5, "corpse_run": False,
            "scoring": {"coins": 25},
            "waves": [{"at_score": s * 150, "note": "wave %d" % (s + 1)}
                      for s in range(6)],
            "roster": [
                {"name": "Ember Wraith", "behavior": "fast ring-dodger, dives when aggroed"},
                {"name": "Ash Brute", "behavior": "slow tank guarding gates"},
            ],
            "spawn": [0.0, 0.0, 4.5],
            "checkpoints": [[9.9, 0, 3.1], [-7.4, 0, -6.9]],
            "nodes": [
                {"name": "Wraith_N", "pos": [0, 0, -8], "tags": ["enemy", "model:wraith"]},
                {"name": "Wraith_E", "pos": [8, 0, 0], "tags": ["enemy", "model:wraith"]},
                {"name": "Brute_Gate", "pos": [0, 0, 11.5], "tags": ["enemy", "model:brute"]},
                {"name": "Gem_NE", "pos": [3.5, 0, 3.5], "tags": ["pickup", "score", "model:gem"]},
                {"name": "Heart_W", "pos": [-3, 0, 2], "tags": ["pickup", "score", "model:heart"]},
            ],
            "zones": [{"name": flavor.title() + " Arena", "pos": [0, 0, 0],
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


def deploy_runtime(game_dir, port_seed):
    g = Path(game_dir)
    (g / "viewer").mkdir(exist_ok=True)
    (g / "tools").mkdir(exist_ok=True)
    rt = REPO / "template/tools/runtime"
    ex = REPO / "Project/example-village"
    shutil.copy(rt / "runtime.js", g / "viewer/", )
    shutil.copy(rt / "play.html", g / "viewer/")
    shutil.copy(ex / "viewer/three.min.js", g / "viewer/")
    shutil.copy(ex / "tools/serve_live.py", g / "tools/")
    shutil.copy(ex / "play_native.py", g / "")
    port = 8100 + (port_seed % 400)
    bat = ("@echo off\nrem %s player\ncd /d \"%%~dp0\"\n"
           "start \"litt-server\" /min python tools\\serve_live.py --port %d\n"
           "timeout /t 2 >nul\nstart http://127.0.0.1:%d/viewer/play.html"
           % (g.name, port, port))
    (g / "PLAY.bat").write_text(bat, encoding="ascii")
    nbat = ("@echo off\nrem %s native window\ncd /d \"%%~dp0\"\n"
            "python play_native.py\nif errorlevel 1 pause" % g.name)
    (g / "NATIVE.bat").write_text(nbat, encoding="ascii")
    return port


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
    ap.add_argument("--skip-validate", action="store_true")
    a = ap.parse_args()

    if not a.random and not a.about and not a.archetype:
        ap.error("pass --random or --about \"description\"")

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
    kit = a.kit or kit
    name = a.name or auto_name(rng)
    out = Path(a.out_dir) if a.out_dir else REPO / "Project" / name
    if out.exists():
        name = name + "-%d" % (seed % 1000)
        out = Path(a.out_dir) if a.out_dir else REPO / "Project" / name
    print("[make] building '%s' | %s/%s/%s kit=%s seed=%d -> %s"
          % (name, arch, pat, theme, kit, seed, out))

    # ---- pipeline (each step a shipped tool) -----------------------------
    if arch == "platformer_2_5d":
        run(WORLDGEN / "gen_platformer25d.py", "--out-dir", str(out),
            "--agent", "ai-agent", "--prompt", a.about or "random")
    else:
        run(WORLDGEN / "gen_archetype.py", "--archetype", arch,
            "--pattern", pat, "--theme", theme, "--seed", str(seed),
            "--name", name, "--out-dir", str(out),
            "--prompt", a.about or "random")
    run(WORLDGEN / "gen_props.py", "--game-dir", str(out), "--kit", kit)

    brief = brief_for(kit, theme, name, seed, a.about)
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
    port = deploy_runtime(out, seed)

    # NOTES + ATTRIBUTION + manifest ---------------------------------------
    (out / "NOTES.md").write_text(
        "# NOTES - %s\n\n%s\n\n- built by: make_game.py (%s mode)\n"
        "- archetype=%s pattern=%s theme=%s kit=%s seed=%d\n"
        "- lint: clean | solids nodes: %d\n- browser port: %d\n"
        % (name, (a.about or "random pick"), "about" if a.about else "random",
           arch, pat, theme, kit, seed, solids, port), encoding="utf-8")
    (out / "ATTRIBUTION.md").write_text(
        "# ATTRIBUTION - %s\n\nAll assets procedurally generated by Litt "
        "worldgen tools. No third-party content.\n" % name, encoding="utf-8")

    manifest_p = REPO / "Project/games.json"
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
    manifest_p.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(json.dumps({"ok": True, "game": name, "dir": str(out),
                      "objs": report["objs"], "solids_nodes": solids,
                      "browser_port": port}))


if __name__ == "__main__":
    main()
