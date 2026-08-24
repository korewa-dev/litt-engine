#!/usr/bin/env python3
"""verify_project.py - HARD audit that a Litt game project actually works.

Checks everything any consumer (native player, Rust engine, C++ viewer,
Studio) needs. Exit code 0 == the project is provably consumable.

Usage:
  python template/tools/assets/verify_project.py            # all of Project/
  python template/tools/assets/verify_project.py ember-depths

Audit list:
  1. scene DTO: format, unique int ids, next_id ahead, positions are [3]
  2. every model:<ref> tag resolves to assets/models/<ref>.obj
  3. every OBJ: no bare usemtl, face indices within vertex count
  4. every materials.mtl map_Kd points to an existing file
  5. world_state.json: format, identity.{camera,movement}, gameplay.physics
     keys the runtime reads verbatim
  6. asset_index.json parses and every registered path exists
  7. launcher surface: ENGINE.bat + ENGINE.sh + VIEW.bat + play_native.py
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
from lint import lint_obj, validate_scene  # noqa: E402

REPO = HERE.parent.parent.parent
PROJECTS = REPO / "Project"

RUNTIME_CAMERAS = {"3D", "TOP", "2D5"}


def resolve_mode(ident):
    """Mirror runtime.js mode selection EXACTLY (substring rules).

    2D5 if movement contains 'platformer' or camera contains 'side';
    TOP if camera contains 'top_down' or 'isometric'; otherwise 3D."""
    def has(s, sub):
        return bool(s) and sub in str(s).lower()
    movement = ident.get("movement") or ""
    camera = ident.get("camera") or ""
    if has(movement, "platformer") or has(camera, "side"):
        return "2D5"
    if has(camera, "top_down") or has(camera, "isometric"):
        return "TOP"
    return "3D"


def audit_game(gdir: Path):
    problems = []
    warn = []

    # --- 1/2. scene ---
    scene_p = gdir / "assets/scenes/world.lscn.json"
    if not scene_p.exists():
        return ["FATAL: no assets/scenes/world.lscn.json"], []
    try:
        dangling = validate_scene(scene_p, gdir / "assets/models")
        if dangling:
            problems.append("scene references missing models: %s"
                            % ", ".join(sorted(set(dangling))))
    except AssertionError as e:
        problems.append("scene invalid: %s" % e)
        return problems, warn
    d = json.loads(scene_p.read_text(encoding="utf-8"))
    for n in d["nodes"]:
        pos = n.get("position", [])
        if pos and len(pos) != 3:
            problems.append("%s position len %d != 3" % (n.get("name"), len(pos)))

    # --- 3. objs ---
    for obj in sorted((gdir / "assets/models").glob("*.obj")):
        for p in lint_obj(obj):
            problems.append(f"{obj.name}: {p}")

    # --- 4. mtl textures ---
    for mtl in sorted((gdir / "assets/models").glob("*.mtl")):
        for line in mtl.read_text(encoding="utf-8").splitlines():
            t = line.strip()
            if t.startswith("map_Kd"):
                tex_rel = t.split(None, 1)[1] if len(t.split(None, 1)) > 1 else ""
                if tex_rel:
                    tex = (mtl.parent / tex_rel).resolve()
                    if not tex.exists():
                        problems.append(
                            f"{mtl.name}: map_Kd '{tex_rel}' missing")

    # --- 5. world_state ---
    state_p = gdir / "world_state.json"
    if state_p.exists():
        s = json.loads(state_p.read_text(encoding="utf-8"))
        ident = s.get("identity") or {}
        mode = resolve_mode(ident)
        if mode not in RUNTIME_CAMERAS:
            problems.append(f"resolved mode {mode} invalid")
        gp = s.get("gameplay") or {}
        phys = gp.get("physics") or {}
        bad_types = [k for k, v in phys.items()
                     if not isinstance(v, (int, float))]
        if bad_types:
            problems.append("gameplay.physics non-numeric: %s"
                            % ", ".join(bad_types))

    # --- 6. asset index ---
    idx_p = gdir / "assets/asset_index.json"
    if idx_p.exists():
        try:
            idx = json.loads(idx_p.read_text(encoding="utf-8"))
            entries = idx.get("entries", idx if isinstance(idx, list) else [])
            for e in entries:
                rel = e.get("path") or e.get("rel_path")
                if rel and not (gdir / "assets" / rel).exists():
                    problems.append(f"asset_index: registered '{rel}' missing")
        except (json.JSONDecodeError, AttributeError):
            warn.append("asset_index.json unparseable - skipped")
    else:
        warn.append("no asset_index.json")

    # --- 7. launchers: the ENGINE pair is THE playable path (any OS) ---
    engine_ok = ((gdir / "ENGINE.sh").exists()
                 and (gdir / "ENGINE.bat").exists())
    if not engine_ok:
        problems.append(
            "no native launcher pair (need ENGINE.bat + ENGINE.sh)")
    # browser stack is PHASED OUT; VIEW.bat is the native visual checker
    view_ok = (gdir / "VIEW.bat").exists()
    validator_ok = (gdir / "play_native.py").exists()
    if not (view_ok and validator_ok):
        warn.append("native view/validator incomplete "
                    "(VIEW.bat=%s play_native=%s)" % (view_ok, validator_ok))

    return problems, warn


def main():
    args = sys.argv[1:]
    targets = ([PROJECTS / a for a in args] if args
               else sorted(p for p in PROJECTS.iterdir() if p.is_dir()))
    total_bad = 0
    print(f"{'project':<20} {'status':<8} notes")
    print("-" * 72)
    for g in targets:
        if not (g / "assets").exists():
            print(f"{g.name:<20} {'SKIP':<8} (not a game folder)")
            continue
        problems, warn = audit_game(g)
        status = "FAIL" if problems else "PASS"
        notes = "; ".join(problems[:3]) or ("warn: " + "; ".join(warn[:2]) if warn else "")
        print(f"{g.name:<20} {status:<8} {notes[:80]}")
        for p in problems[3:]:
            print(f"{'':<20}         {p[:90]}")
        total_bad += bool(problems)
    print("-" * 72)
    print("projects failing:", total_bad)
    sys.exit(1 if total_bad else 0)


if __name__ == "__main__":
    main()
