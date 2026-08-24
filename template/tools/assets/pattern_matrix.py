#!/usr/bin/env python3
"""pattern_matrix.py - adversarial pipeline regression for Litt.

For EVERY gen_archetype layout pattern x sampled themes:
  1. generate a fresh world (gen -> props -> enrich)
  2. lint every emitted OBJ: bare usemtl, out-of-range face indices,
     non-triangulated faces are tolerated but counted
  3. validate scene DTO invariants (ids, next_id, model refs resolvable)
  4. run the native player headless --frames N and require success
Any failure prints the exact pattern/theme/seed to reproduce.

Run: python template/tools/assets/pattern_matrix.py [--keep]
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).parent
WORLDGEN = HERE.parent / "worldgen"
REPO = HERE.parent.parent.parent

PATTERNS = ["arena_ring", "corridor_run", "hub_spoke", "grid_board",
            "spline_track", "room_graph"]
THEMES = ["dark_fantasy", "post_apocalypse", "cyberpunk_neon", "haunted_estate"]
KITS = {"arena_ring": "survivor", "corridor_run": "survivor",
        "hub_spoke": "souls", "grid_board": "survivor",
        "spline_track": "platformer", "room_graph": "souls"}


def sh(*args, **kw):
    r = subprocess.run(args, capture_output=True, text=True, **kw)
    return r


def lint_obj(path):
    """Return list of problems for one OBJ file."""
    problems = []
    nv = 0
    usemtl_bare = 0
    max_idx = 0
    for ln in path.read_text(encoding="utf-8").splitlines():
        t = ln.strip()
        if not t or t.startswith("#") or t.startswith("g ") or t.startswith("o "):
            continue
        p = t.split()
        if p[0] == "v":
            nv += 1
        elif p[0] == "usemtl":
            if len(p) < 2 or not p[1].strip():
                usemtl_bare += 1
        elif p[0] == "f":
            for seg in p[1:]:
                idx = int(seg.split("/")[0])
                if idx < 0:
                    idx = nv + idx + 1
                max_idx = max(max_idx, idx)
    if usemtl_bare:
        problems.append("%d bare usemtl" % usemtl_bare)
    if max_idx > nv:
        problems.append("face index %d > vertex count %d" % (max_idx, nv))
    return problems


def validate_scene(scene_path, models_dir):
    d = json.loads(Path(scene_path).read_text(encoding="utf-8"))
    assert d.get("format") == "litt-scene", "bad format"
    ids = set()
    for n in d["nodes"]:
        assert isinstance(n.get("id"), int) and n["id"] not in ids, \
            "duplicate/invalid id %r" % n.get("id")
        ids.add(n["id"])
        for tag in n.get("tags", []):
            if tag.startswith("model:"):
                ref = tag[6:]
                if ref and not (models_dir / (ref + ".obj")).exists():
                    return ["scene references missing model '%s'" % ref]
    assert d.get("next_id", 0) >= max(ids, default=0) + 1, "next_id behind"
    return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("--frames", type=int, default=6)
    ap.add_argument("--seed-base", type=int, default=100)
    a = ap.parse_args()

    failures = []
    total = 0
    base = Path(tempfile.mkdtemp(prefix="litt_matrix_"))
    try:
        for pattern in PATTERNS:
            theme = THEMES[PATTERNS.index(pattern) % len(THEMES)]
            seed = a.seed_base + PATTERNS.index(pattern)
            gdir = base / pattern
            total += 1

            r = sh(sys.executable, str(WORLDGEN / "gen_archetype.py"),
                   "--archetype", "roguelite", "--pattern", pattern,
                   "--theme", theme, "--seed", str(seed),
                   "--name", pattern, "--out-dir", str(gdir))
            if r.returncode != 0:
                failures.append("[generate] %s/%s: %s"
                                % (pattern, theme, r.stderr[-300:]))
                continue

            kit = KITS[pattern]
            r = sh(sys.executable, str(WORLDGEN / "gen_props.py"),
                   "--game-dir", str(gdir), "--kit", kit)
            if r.returncode != 0:
                failures.append("[props] %s: %s" % (pattern, r.stderr[-300:]))
                continue

            # OBJ lint across all models
            obj_problems = []
            models = gdir / "assets" / "models"
            for obj in models.glob("*.obj"):
                for p in lint_obj(obj):
                    obj_problems.append("%s.obj: %s" % (obj.name, p))
            scene_problems = validate_scene(
                gdir / "assets" / "scenes" / "world.lscn.json", models)
            if obj_problems or scene_problems:
                failures.append("[lint] %s:\n  %s"
                                % (pattern,
                                   "\n  ".join(obj_problems + scene_problems)))

            # enrich with ember brief (exercises spawn/checkpoints/zones)
            r = sh(sys.executable, str(WORLDGEN / "enrich_game.py"),
                   "--game-dir", str(gdir),
                   "--brief", str(REPO / "Project/ember-depths/brief.json"),
                   "--seed", "9")
            if r.returncode != 0:
                failures.append("[enrich] %s: %s" % (pattern, r.stderr[-300:]))
                continue

            shutil.copy(REPO / "Project/ember-depths/play_native.py",
                        gdir / "play_native.py")
            r = sh(sys.executable, str(gdir / "play_native.py"),
                   "--project", str(gdir), "--frames", str(a.frames))
            ok = r.returncode == 0 and "rendered" in r.stdout
            print(("PASS  " if ok else "FAIL  ")
                  + "%-10s theme=%-18s seed=%d | %s"
                  % (pattern, theme, seed,
                     (r.stdout.strip().splitlines() or ["?"])[-1][:80]))
            if not ok:
                failures.append("[native] %s: %s | %s"
                                % (pattern, r.stdout[-200:], r.stderr[-400:]))
        # platformer generator too
        gdir = base / "platformer"
        r = sh(sys.executable, str(WORLDGEN / "gen_platformer25d.py"),
               "--out-dir", str(gdir), "--agent", "matrix")
        ok = r.returncode == 0
        print(("PASS  " if ok else "FAIL  ") + "platformer25d")
        if not ok:
            failures.append("[platformer] %s" % r.stderr[-300:])
        else:
            total += 1
    finally:
        if not a.keep:
            shutil.rmtree(base, ignore_errors=True)

    print("\n%d/%d patterns clean" % (total - len(failures), total))
    if failures:
        print("\nFAILURES:")
        for f in failures:
            print("-", f)
        sys.exit(1)


if __name__ == "__main__":
    main()
