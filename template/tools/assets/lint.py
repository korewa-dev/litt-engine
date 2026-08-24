#!/usr/bin/env python3
"""lint.py - shared asset/scene validation used by every Litt pipeline.

Importable by pattern_matrix.py, make_game.py, selftest.py and agents.
Keeps the "every tool works with every tool" contract in ONE place."""
from pathlib import Path


def lint_obj(path):
    """Return list of problems for one OBJ file (empty == clean)."""
    problems = []
    nv = 0
    usemtl_bare = 0
    max_idx = 0
    for ln in Path(path).read_text(encoding="utf-8").splitlines():
        t = ln.strip()
        if not t or t.startswith("#") or t.startswith(("g ", "o ")):
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
    if nv and max_idx > nv:
        problems.append("face index %d > vertex count %d" % (max_idx, nv))
    return problems


def validate_scene(scene_path, models_dir=None):
    """Scene DTO invariants + dangling model:<ref> check."""
    import json
    d = json.loads(Path(scene_path).read_text(encoding="utf-8"))
    assert d.get("format") == "litt-scene", "bad format"
    ids = set()
    dangling = []
    models_dir = Path(models_dir) if models_dir else \
        Path(scene_path).parent.parent / "models"
    for n in d["nodes"]:
        nid = n.get("id")
        assert isinstance(nid, int) and nid not in ids, \
            "duplicate/invalid id %r" % nid
        ids.add(nid)
        for tag in n.get("tags", []):
            if tag.startswith("model:") and tag[6:]:
                if not (models_dir / (tag[6:] + ".obj")).exists():
                    dangling.append(tag[6:])
    assert d.get("next_id", 0) >= max(ids, default=0) + 1, "next_id behind"
    return dangling


def lint_game(game_dir):
    """Lint every OBJ + the scene of a whole project. Returns dict report."""
    g = Path(game_dir)
    report = {"objs": 0, "problems": [], "dangling_refs": []}
    models = g / "assets" / "models"
    for obj in sorted(models.glob("*.obj")):
        report["objs"] += 1
        for p in lint_obj(obj):
            report["problems"].append("%s: %s" % (obj.name, p))
    scene = g / "assets" / "scenes" / "world.lscn.json"
    if scene.exists():
        report["dangling_refs"] = validate_scene(scene, models)
    return report


def solid_count(scene_path):
    """How many walkable-surface nodes (what native player treats as solids)."""
    import json
    d = json.loads(Path(scene_path).read_text(encoding="utf-8"))
    walk = {"floor", "level", "board", "track", "hub", "terrain", "platform"}
    n = 0
    for node in d["nodes"]:
        tags = set(node.get("tags", []))
        if tags & walk:
            n += 1
    return n
