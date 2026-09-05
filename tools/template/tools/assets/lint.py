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


def obj_vertex_centroid_xz(path):
    """Mean (x, z) over all 'v' lines of one OBJ (cheap streaming parse);
    None when the file has no parsable vertices."""
    sx = sz = 0.0
    n = 0
    for ln in Path(path).read_text(encoding="utf-8").splitlines():
        t = ln.strip()
        if t.startswith("v "):
            p = t.split()
            try:
                sx += float(p[1])
                sz += float(p[3])
                n += 1
            except (IndexError, ValueError):
                return None
    if not n:
        return None
    return (sx / n, sz / n)


def lint_double_transform(scene_path, models_dir=None,
                          node_off_tol=0.5, centroid_tol=1.5):
    """Audit 2.3 origin guard: warn when a NON-terrain scene node sits more
    than `node_off_tol` away from the origin (|x| or |z|) while an OBJ it
    references carries its own vertex centroid further than `centroid_tol`
    from origin - the baked-world-coords + node-offset double-transform
    smell that renders/sims displace ~2x. Centroid distance is measured in
    the x/z plane to match worldkit's base-center convention (tall meshes
    legitimately ride up y). Terrain-tagged nodes are the SANCTIONED
    exception: emit_chunk bakes WORLD-space vertices by design and chunk
    nodes must sit at identity anyway. Returns warning strings (empty ==
    clean); callers fold them into their problems list."""
    import json
    import math
    scene = Path(scene_path)
    if not scene.exists():
        return []
    models_dir = Path(models_dir) if models_dir else \
        scene.parent.parent / "models"
    d = json.loads(scene.read_text(encoding="utf-8"))
    out = []
    for n in d.get("nodes", []):
        tags = n.get("tags", [])
        if "terrain" in tags:
            continue  # sanctioned terrain-chunk exception
        refs = [t[6:] for t in tags if t.startswith("model:") and t[6:]]
        pos = n.get("position") or [0, 0, 0]
        if not refs or len(pos) < 3:
            continue
        off = max(abs(float(pos[0])), abs(float(pos[2])))
        if off <= node_off_tol:
            continue
        for ref in refs:
            cxz = obj_vertex_centroid_xz(models_dir / (ref + ".obj"))
            if cxz is None:
                continue
            dist = math.sqrt(cxz[0] * cxz[0] + cxz[1] * cxz[1])
            if dist > centroid_tol:
                out.append(
                    "node %s -> %s.obj: possible double-transform "
                    "(node xz offset %.2f but OBJ vertex centroid is "
                    "%.2f from its own origin)" % (n.get("name"), ref,
                                                   off, dist))
    return out


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
        # audit 2.3: static double-transform smell check, same problems list
        for w in lint_double_transform(scene, models):
            report["problems"].append("scene: %s" % w)
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
