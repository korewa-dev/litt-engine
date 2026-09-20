#!/usr/bin/env python3
"""Generate representative Litt worlds and enforce low-resource quality budgets."""
import argparse, hashlib, json, subprocess, sys, tempfile, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = [
    ("archetype_arena", "gen_archetype.py", ["--archetype","bullet_hell","--pattern","arena_ring","--theme","dark_fantasy","--seed","101"]),
    ("archetype_corridor", "gen_archetype.py", ["--archetype","precision_action","--pattern","corridor_run","--theme","cyberpunk_neon","--seed","102"]),
    ("archetype_hub", "gen_archetype.py", ["--archetype","walking_simulator","--pattern","hub_spoke","--theme","deep_forest","--seed","103"]),
    ("archetype_board", "gen_archetype.py", ["--archetype","grid_tactics","--pattern","grid_board","--theme","minimalist_abstract","--seed","104"]),
    ("archetype_track", "gen_archetype.py", ["--archetype","kart_racer","--pattern","spline_track","--theme","retro_scifi","--seed","105"]),
    ("archetype_rooms", "gen_archetype.py", ["--archetype","dungeon_crawler","--pattern","room_graph","--theme","underground_caves","--seed","106"]),
    ("soulslike", "gen_soulslike.py", ["--seed","207"]),
    ("space", "gen_space.py", ["--seed","308"]),
    ("tabletop", "gen_tabletop.py", ["--seed","409"]),
    ("platformer25d", "gen_platformer25d.py", ["--seed","510"]),
]
MAX_TOTAL_BYTES = 3 * 1024 * 1024
MAX_FILES = 160
MAX_MODELS = 56
MAX_UNIQUE_MODELS = 48
MAX_SECONDS = 8.0

VOLATILE_NAMES = {"LIVE_LOG.md"}
VOLATILE_STATE_KEYS = {"updated"}

def canonical_bytes(path):
    rel = path.name
    if rel in VOLATILE_NAMES:
        return b""
    data = path.read_bytes()
    if path.name == "world_state.json":
        try:
            obj = json.loads(data)
            for key in VOLATILE_STATE_KEYS: obj.pop(key, None)
            return json.dumps(obj, sort_keys=True, separators=(",",":")).encode()
        except Exception:
            pass
    return data

def digest_tree(root):
    h=hashlib.sha256()
    for p in sorted(x for x in root.rglob("*") if x.is_file()):
        rel=p.relative_to(root).as_posix()
        if p.name in VOLATILE_NAMES: continue
        h.update(rel.encode()+b"\0"+canonical_bytes(p)+b"\0")
    return h.hexdigest()

def metrics(root, seconds):
    files=[p for p in root.rglob("*") if p.is_file()]
    models=list((root/"assets"/"models").glob("*.obj"))
    scene=json.loads((root/"assets"/"scenes"/"world.lscn.json").read_text())
    refs=[]
    tag_counts={}
    for n in scene.get("nodes",[]):
        for t in n.get("tags",[]):
            tag_counts[t]=tag_counts.get(t,0)+1
            if t.startswith("model:"): refs.append(t[6:])
    return {
        "seconds":round(seconds,3),"bytes":sum(p.stat().st_size for p in files),
        "files":len(files),"models":len(models),"unique_model_refs":len(set(refs)),
        "nodes":len(scene.get("nodes",[])),"model_instances":len(refs),
        "goals":tag_counts.get("goal",0),"hazards":tag_counts.get("hazard",0),
        "enemies":tag_counts.get("enemy",0),"pickups":tag_counts.get("pickup",0),
    }

def run_case(name, script, args, root):
    out=root/name
    cmd=[sys.executable,str(HERE/script),"--out-dir",str(out)]+args
    t=time.perf_counter(); r=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
    dt=time.perf_counter()-t
    if r.returncode:
        raise RuntimeError("%s failed: %s"%(name,r.stderr[-2000:]))
    return out,metrics(out,dt)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--json-out")
    a=ap.parse_args()
    report={"cases":{},"failures":[]}
    with tempfile.TemporaryDirectory(prefix="litt-wg-") as td:
        root=Path(td)
        for name,script,args in CASES:
            out,m=run_case(name,script,args,root)
            d1=digest_tree(out)
            out2=root/(name+"_repeat")
            cmd=[sys.executable,str(HERE/script),"--out-dir",str(out2)]+args
            r=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
            if r.returncode: report["failures"].append(name+": repeat generation failed")
            d2=digest_tree(out2) if r.returncode==0 else ""
            m["deterministic"]=d1==d2
            report["cases"][name]=m
            checks=[
                (m["deterministic"],"not deterministic"),
                (m["seconds"]<=MAX_SECONDS,"generation %.2fs > %.2fs"%(m["seconds"],MAX_SECONDS)),
                (m["bytes"]<=MAX_TOTAL_BYTES,"output %d bytes > %d"%(m["bytes"],MAX_TOTAL_BYTES)),
                (m["files"]<=MAX_FILES,"files %d > %d"%(m["files"],MAX_FILES)),
                (m["models"]<=MAX_MODELS,"models %d > %d"%(m["models"],MAX_MODELS)),
                (m["unique_model_refs"]<=MAX_UNIQUE_MODELS,"unique models %d > %d"%(m["unique_model_refs"],MAX_UNIQUE_MODELS)),
                (m["nodes"]>0,"empty scene"),
            ]
            for ok,msg in checks:
                if not ok: report["failures"].append(name+": "+msg)
            print("%-20s %6.3fs %7.1fKB %3df %3dm %3dn inst=%3d det=%s"%(
                name,m["seconds"],m["bytes"]/1024,m["files"],m["models"],m["nodes"],m["model_instances"],m["deterministic"]))
    if a.json_out: Path(a.json_out).write_text(json.dumps(report,indent=2)+"\n")
    if report["failures"]:
        print("\nFAILURES:")
        for x in report["failures"]: print(" - "+x)
        return 1
    print("\nWorldGen quality/resource gate passed.")
    return 0
if __name__=="__main__": raise SystemExit(main())
