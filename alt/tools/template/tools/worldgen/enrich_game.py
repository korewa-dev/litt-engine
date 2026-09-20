#!/usr/bin/env python3
"""enrich_game.py - upgrade a generated litt world into a FEATURE-RICH game.

The generators (gen_archetype / gen_platformer25d / gen_soulslike ...) build
geometry. This tool layers GAMEPLAY on top, driven by a JSON *brief* that any
AI can author. It never touches meshes: it wires nodes, rules and tuning into
the two files the runtimes consume.

Usage:
  python enrich_game.py --game-dir Project/ember-depths --brief brief.json [--seed 7]

Brief schema (all optional, unknown keys are kept verbatim in state):
{
  "objective": "Survive 6 waves of the Ember Legion",
  "side_objectives": ["Light all 4 braziers", ...],
  "physics": {"gravity": 26, "jump_velocity": 9, "run_speed": 7,
               "coyote_time_s": 0.12},
  "enemy_aggro_m": 8.0,
  "corpse_run": true,
  "scoring": {"coins": 25},
  "waves": [{"at_score": 100, "note": "wraiths speed up"}],
  "roster": [{"name": "Ember Wraith", "behavior": "fast dodger"}],
  "spawn": [x, y, z],                       # node tagged player
  "checkpoints": [[x,y,z], ...],            # lit checkpoint shrines
  "nodes": [                                 # extra tagged nodes
     {"name": "Brazier_01", "pos": [x,y,z], "tags": ["pickup","model:coin"]},
  ],
  "zones": [                                  # named regions (engine areas)
     {"name": "Crypt", "pos": [x,y,z], "radius": 18, "tags": ["music:dread"]}
  ]
}

Deterministic: same brief + seed => same output. Logs to LIVE_LOG.md.
"""
import argparse
import datetime
import json
from pathlib import Path


def load_json(p: Path):
    return json.loads(p.read_text(encoding="utf-8"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--brief", required=True)
    ap.add_argument("--seed", type=int, default=1337)
    ap.add_argument("--agent", default="ai-agent")
    a = ap.parse_args()

    root = Path(a.game_dir)
    state_path = root / "world_state.json"
    scene_path = root / "assets" / "scenes" / "world.lscn.json"
    state = load_json(state_path)
    scene = load_json(scene_path)
    brief = load_json(Path(a.brief))

    gp = state.setdefault("gameplay", {})
    added_nodes = []

    # ---- rules & tuning -------------------------------------------------
    for key in ("objective", "enemy_aggro_m", "corpse_run",
                "spawn", "lives", "interact_radius_m", "kill_radius_m"):
        if key in brief:
            gp[key] = brief[key]
    if "scoring" in brief:
        gp["scoring"] = brief["scoring"]
    if "physics" in brief:
        gp.setdefault("physics", {}).update(brief["physics"])
    for key in ("side_objectives", "waves", "roster"):
        if key in brief:
            gp[key] = brief[key]

    # ---- scene surgery ----------------------------------------------------
    nodes = scene.setdefault("nodes", [])
    ids = [n.get("id") for n in nodes if isinstance(n.get("id"), int)]
    if len(ids) != len(set(ids)):
        raise ValueError("scene contains duplicate node ids")
    next_id = max([scene.get("next_id", 1), 1] + [i + 1 for i in ids])
    existing_by_name = {n.get("name"): n for n in nodes if n.get("name")}

    def add(name, pos, tags, scale=(1.0, 1.0, 1.0)):
        nonlocal next_id
        values = {
            "position": [round(float(c), 3) for c in pos],
            "rotation": [0.0, 0.0, 0.0, 1.0],
            "scale": [float(s) for s in scale],
            "visible": True, "layer": 0, "tags": list(tags),
        }
        if name in existing_by_name:
            node = existing_by_name[name]
            node.update(values)
            return node
        node = {
            "name": name, "id": next_id, "parent": 0, "children": [],
            **values,
        }
        next_id += 1
        nodes.append(node)
        existing_by_name[name] = node
        added_nodes.append((name, tags))
        return node

    n = 0
    for c in brief.get("checkpoints", []):
        n += 1
        add("Checkpoint_%02d" % n, c, ["checkpoint", "poi"])

    for spec in brief.get("nodes", []):
        node = add(spec["name"], spec["pos"], spec["tags"],
                   spec.get("scale", (1, 1, 1)))
        # story layer: item/roster flavor text rides along as a poi notice
        if node is not None:
            poi = spec.get("poi")
            if poi and "poi" not in node.get("tags", []):
                node.setdefault("tags", []).append("poi")

    for z in brief.get("zones", []):
        r = float(z.get("radius", 15))
        # engine convention: area radius == scale.x * 10
        add(z["name"], z["pos"], ["area"] + list(z.get("tags", [])),
            scale=(r / 10.0, 1.0, 1.0))

    if "spawn" in brief:
        add("Player_Start", brief["spawn"], ["player", "start"])

    scene["next_id"] = next_id

    # ---- write order matters: scene first, state LAST (viewers poll) -----
    scene_path.write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8")
    state.setdefault("meta", {})["enriched_seed"] = a.seed
    state["updated"] = datetime.datetime.now().isoformat(timespec="seconds")
    state_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")

    log = root / "LIVE_LOG.md"
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    with open(log, "a", encoding="utf-8") as fh:
        fh.write("\n---\n\n## %s - ENRICH by %s (AI)\n" % (stamp, a.agent))
        fh.write("- brief: %s (seed %d)\n" % (Path(a.brief).name, a.seed))
        fh.write("- objective: %s\n" % gp.get("objective", "-"))
        fh.write("- +%d nodes (checkpoints/pickups/zones/spawn)\n" % len(added_nodes))

    print("[enrich] %s -> +%d nodes | objective: %s"
          % (root.name, len(added_nodes), str(gp.get("objective"))[:60]))


if __name__ == "__main__":
    main()
