#!/usr/bin/env python3
"""Deterministic, low-resource WorldGen and generated-game quality gate."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.resolve().parents[3]
NATIVE_BIN = REPO / "src" / "native" / "bin"

# Representative patterns plus deliberately awkward/large seeds. Keep this
# corpus cheap enough for every PR while covering all supported generator kinds.
CASES = [
    ("archetype_arena", "gen_archetype.py", ["--archetype","bullet_hell","--pattern","arena_ring","--theme","dark_fantasy","--seed","101"]),
    ("archetype_corridor", "gen_archetype.py", ["--archetype","precision_action","--pattern","corridor_run","--theme","cyberpunk_neon","--seed","102"]),
    ("archetype_hub", "gen_archetype.py", ["--archetype","walking_simulator","--pattern","hub_spoke","--theme","deep_forest","--seed","103"]),
    ("archetype_board", "gen_archetype.py", ["--archetype","grid_tactics","--pattern","grid_board","--theme","minimalist_abstract","--seed","104"]),
    ("archetype_track", "gen_archetype.py", ["--archetype","kart_racer","--pattern","spline_track","--theme","retro_scifi","--seed","105"]),
    ("archetype_rooms", "gen_archetype.py", ["--archetype","dungeon_crawler","--pattern","room_graph","--theme","underground_caves","--seed","106"]),
    ("archetype_zero", "gen_archetype.py", ["--archetype","roguelite","--pattern","grid_board","--theme","dark_fantasy","--seed","0"]),
    ("archetype_large_seed", "gen_archetype.py", ["--archetype","open_world_survival","--pattern","arena_ring","--theme","arctic_expanse","--seed","2147483647"]),
    ("soulslike", "gen_soulslike.py", ["--seed","207"]),
    ("soulslike_complex", "gen_soulslike.py", ["--seed","65537"]),
    ("space", "gen_space.py", ["--seed","308"]),
    ("tabletop", "gen_tabletop.py", ["--seed","409"]),
    ("platformer25d", "gen_platformer25d.py", ["--seed","510"]),
    ("platformer25d_complex", "gen_platformer25d.py", ["--seed","104729"]),
]

MAX_TOTAL_BYTES = 3 * 1024 * 1024
MAX_FILES = 160
MAX_MODELS = 56
MAX_UNIQUE_MODELS = 48
MAX_SECONDS = 8.0
MAX_TOTAL_NODES = 512
MAX_MODEL_INSTANCES = 512
VOLATILE_NAMES = {"LIVE_LOG.md"}
VOLATILE_STATE_KEYS = {"updated"}
TEXT_SUFFIXES = {".json", ".md", ".txt", ".csv", ".obj", ".mtl", ".py", ".sh", ".bat"}

def _canonical_json(obj):
    if isinstance(obj, dict):
        return {str(k): _canonical_json(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_canonical_json(v) for v in obj]
    if isinstance(obj, str):
        return obj.replace("\\\\", "/")
    return obj

def canonical_bytes(path):
    """OS-independent bytes for deterministic comparison and CI digests."""
    if path.name in VOLATILE_NAMES:
        return b""
    data = path.read_bytes()
    if path.suffix.lower() == ".json":
        try:
            obj = json.loads(data.decode("utf-8"))
            if path.name == "world_state.json" and isinstance(obj, dict):
                for key in VOLATILE_STATE_KEYS:
                    obj.pop(key, None)
            obj = _canonical_json(obj)
            return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        except (UnicodeDecodeError, json.JSONDecodeError):
            pass
    if path.suffix.lower() in TEXT_SUFFIXES:
        try:
            return data.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
        except UnicodeDecodeError:
            pass
    return data

def digest_tree(root):
    h = hashlib.sha256()
    for p in sorted((x for x in root.rglob("*") if x.is_file()),
                    key=lambda x: x.relative_to(root).as_posix()):
        if p.name in VOLATILE_NAMES:
            continue
        rel = p.relative_to(root).as_posix()
        h.update(rel.encode("utf-8") + b"\0" + canonical_bytes(p) + b"\0")
    return h.hexdigest()

def reference_problems(root):
    """Catch references that pass generation but fail on another machine."""
    problems = []
    files = [p for p in root.rglob("*") if p.is_file()]
    rels = {p.relative_to(root).as_posix() for p in files}
    folded = {}
    for rel in rels:
        key = rel.casefold()
        if key in folded and folded[key] != rel:
            problems.append("case-colliding paths: %s / %s" % (folded[key], rel))
        folded[key] = rel

    scene_path = root / "assets" / "scenes" / "world.lscn.json"
    try:
        scene = json.loads(scene_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return ["scene unreadable: %s" % exc]

    for node in scene.get("nodes", []):
        for tag in node.get("tags", []):
            if tag.startswith("model:"):
                rel = "assets/models/%s.obj" % tag[6:]
                if rel not in rels:
                    problems.append("dangling scene model: %s" % rel)

    index_path = root / "assets" / "asset_index.json"
    if index_path.exists():
        try:
            idx = json.loads(index_path.read_text(encoding="utf-8"))
            entries = idx.get("entries", idx if isinstance(idx, list) else [])
            for entry in entries:
                rel = entry.get("path") or entry.get("rel_path")
                if rel:
                    candidates = {rel.replace("\\", "/"), "assets/" + rel.replace("\\", "/")}
                    if not any(c in rels for c in candidates):
                        problems.append("dangling asset index: %s" % rel)
        except (json.JSONDecodeError, AttributeError) as exc:
            problems.append("asset index unreadable: %s" % exc)

    for mtl in (root / "assets" / "models").glob("*.mtl"):
        for line in mtl.read_text(encoding="utf-8").splitlines():
            bits = line.strip().split(None, 1)
            if bits and bits[0] == "map_Kd" and len(bits) == 2:
                target = (mtl.parent / bits[1]).resolve()
                try:
                    target.relative_to(root.resolve())
                except ValueError:
                    problems.append("texture escapes project: %s" % bits[1])
                    continue
                if not target.is_file():
                    problems.append("dangling texture: %s -> %s" % (mtl.name, bits[1]))
    return sorted(set(problems))

def metrics(root, seconds):
    files = [p for p in root.rglob("*") if p.is_file()]
    models = list((root / "assets" / "models").glob("*.obj"))
    scene = json.loads((root / "assets" / "scenes" / "world.lscn.json").read_text(encoding="utf-8"))
    refs, tag_counts = [], {}
    for node in scene.get("nodes", []):
        for tag in node.get("tags", []):
            tag_counts[tag] = tag_counts.get(tag, 0) + 1
            if tag.startswith("model:"):
                refs.append(tag[6:])
    return {
        "seconds": round(seconds, 3),
        "bytes": sum(p.stat().st_size for p in files),
        "files": len(files),
        "models": len(models),
        "unique_model_refs": len(set(refs)),
        "nodes": len(scene.get("nodes", [])),
        "model_instances": len(refs),
        "goals": tag_counts.get("goal", 0),
        "hazards": tag_counts.get("hazard", 0),
        "enemies": tag_counts.get("enemy", 0),
        "pickups": tag_counts.get("pickup", 0),
    }

def run_generator(script, args, out):
    cmd = [sys.executable, str(HERE / script), "--out-dir", str(out)] + args
    started = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    elapsed = time.perf_counter() - started
    if proc.returncode:
        raise RuntimeError("%s failed: %s" % (script, proc.stderr[-2000:]))
    return elapsed

def find_littcli(explicit):
    if explicit:
        return Path(explicit)
    for name in ("littcli.exe", "littcli"):
        p = NATIVE_BIN / name
        if p.is_file():
            return p
    return None

def native_validate(cli, project):
    proc = subprocess.run([str(cli), "validate", str(project), "--frames", "60"],
                          capture_output=True, text=True, timeout=30)
    return proc.returncode == 0, (proc.stdout + "\n" + proc.stderr)[-2000:]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out")
    ap.add_argument("--littcli", help="native validator; auto-detected when built")
    ap.add_argument("--require-native", action="store_true",
                    help="fail if littcli is unavailable")
    args = ap.parse_args()

    cli = find_littcli(args.littcli)
    report = {
        "schema": 2,
        "platform": {"os": os.name, "sys_platform": sys.platform},
        "limits": {
            "bytes": MAX_TOTAL_BYTES, "files": MAX_FILES, "models": MAX_MODELS,
            "unique_model_refs": MAX_UNIQUE_MODELS, "nodes": MAX_TOTAL_NODES,
            "model_instances": MAX_MODEL_INSTANCES, "seconds": MAX_SECONDS,
        },
        "native_validator": bool(cli),
        "cases": {},
        "failures": [],
    }
    if args.require_native and not cli:
        report["failures"].append("native validator required but littcli was not found")

    with tempfile.TemporaryDirectory(prefix="litt-wg-") as td:
        root = Path(td)
        for name, script, case_args in CASES:
            try:
                out = root / name
                elapsed = run_generator(script, case_args, out)
                m = metrics(out, elapsed)
                m["digest"] = digest_tree(out)
                m["reference_problems"] = reference_problems(out)

                repeat = root / (name + "_repeat")
                run_generator(script, case_args, repeat)
                m["deterministic"] = m["digest"] == digest_tree(repeat)

                if cli:
                    m["native_valid"], detail = native_validate(cli, out)
                    if not m["native_valid"]:
                        m["native_error"] = detail
                else:
                    m["native_valid"] = None

                report["cases"][name] = m
                checks = [
                    (m["deterministic"], "not deterministic"),
                    (not m["reference_problems"], "dangling/cross-OS references: %s" % "; ".join(m["reference_problems"][:4])),
                    (m["seconds"] <= MAX_SECONDS, "generation %.2fs > %.2fs" % (m["seconds"], MAX_SECONDS)),
                    (m["bytes"] <= MAX_TOTAL_BYTES, "output %d bytes > %d" % (m["bytes"], MAX_TOTAL_BYTES)),
                    (m["files"] <= MAX_FILES, "files %d > %d" % (m["files"], MAX_FILES)),
                    (m["models"] <= MAX_MODELS, "models %d > %d" % (m["models"], MAX_MODELS)),
                    (m["unique_model_refs"] <= MAX_UNIQUE_MODELS, "unique models %d > %d" % (m["unique_model_refs"], MAX_UNIQUE_MODELS)),
                    (m["nodes"] > 0, "empty scene"),
                    (m["nodes"] <= MAX_TOTAL_NODES, "nodes %d > %d" % (m["nodes"], MAX_TOTAL_NODES)),
                    (m["model_instances"] <= MAX_MODEL_INSTANCES, "instances %d > %d" % (m["model_instances"], MAX_MODEL_INSTANCES)),
                    (m["native_valid"] is not False, "native validation failed"),
                ]
                for ok, msg in checks:
                    if not ok:
                        report["failures"].append(name + ": " + msg)
                print("%-24s %6.3fs %7.1fKB %3df %3dm %3dn inst=%3d det=%s native=%s" % (
                    name, m["seconds"], m["bytes"] / 1024, m["files"], m["models"],
                    m["nodes"], m["model_instances"], m["deterministic"], m["native_valid"]))
            except Exception as exc:
                report["failures"].append(name + ": " + str(exc))
                print("%-24s ERROR %s" % (name, exc))

    if args.json_out:
        Path(args.json_out).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if report["failures"]:
        print("\nFAILURES:")
        for failure in report["failures"]:
            print(" - " + failure)
        return 1
    print("\nWorldGen quality/resource/native gate passed.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
