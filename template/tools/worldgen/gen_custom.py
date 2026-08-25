#!/usr/bin/env python3
"""CUSTOM WORLD SCAFFOLD - the "no preset fits" starting point (Litt Engine).

This is the minimal complete example of a WorldKit generator. It produces a
small valid world (terrain patch + one monument) so you can verify plumbing,
then you replace the marked sections with YOUR game.

Workflow for any new game idea:
  1. Check design_rules.json - maybe an archetype already maps to a generator.
  2. Copy THIS file to gen_yourgame.py.
  3. Edit PALETTE, build_world() (your geometry), GAMEPLAY (your rules).
  4. Run it; iterate. Everything downstream (index/scene/state/log) is automatic.

Usage: python gen_custom.py --name my_world [--out-dir .] [--agent ai]
"""
import argparse
import datetime
from pathlib import Path

from worldkit import (Rng, fbm, MeshBuilder, write_mtl_for, emit_chunk,
                      register_index, write_scene, write_state, append_log,
                      save_prop)

# TODO: rename and set your palette (RGB 0..1). Keep it under ~12 materials.
PALETTE = {
  "ground":   (0.36, 0.42, 0.34),
  "monument": (0.70, 0.58, 0.30),
  "accent":   (0.85, 0.35, 0.25),
}

# TODO: your gameplay rules as data - the viewer and future code read these.
GAMEPLAY = {
  "genre": "custom",
  "objective": "describe what the player tries to do here",
  "rules": ["rule one", "rule two"],
}

def height_fn(wx, wz, seed):
    """TODO: terrain shape. fBm is the default; see procedural_asset_math.md."""
    return fbm(wx * 0.08, wz * 0.08, seed) * 1.5

def build_monument():
    """TODO: replace with your own props using cookbook primitives."""
    mb = MeshBuilder()
    base = mb; base.begin("base", "monument")
    base.box(0, 0.4, 0, 1.2, 0.4, 1.2)
    mb.begin("spire", "accent")
    mb.pyramid(0, 0.8, 0, 0.7, 0.7, 2.2)
    return mb

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="my_world")
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--radius", type=int, default=1)
    ap.add_argument("--chunk-size", type=float, default=16.0)
    ap.add_argument("--res", type=int, default=12)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", PALETTE)
    made = []; placed = []; registry = []

    # --- terrain chunks (seamless: heights sampled in world space) ---------
    for x in range(-a.radius, a.radius + 1):
        for z in range(-a.radius, a.radius + 1):
            cid = "chunk_%d_%d" % (x, z)
            mb = MeshBuilder()
            emit_chunk(mb, "ground", x, z, a.chunk_size, a.res, a.seed, height_fn)
            obj_text, nv, nf = mb.to_obj(cid, "materials")
            p = models / (cid + ".obj")
            if not p.exists():
                p.write_text(obj_text, encoding="utf-8"); made.append(cid + ".obj")
            registry.append((cid, "models/" + cid + ".obj"))
            # AUDIT 2.1 fix: emit_chunk bakes WORLD-space vertices by design,
            # so per worldkit's documented rule the chunk node MUST sit at
            # identity [0,0,0] - never offset both verts and node.
            placed.append((cid, [0, 0, 0], 0, ["terrain"]))
    for cid, rel in registry:
        register_index(assets_dir, cid, rel)

    # --- props -------------------------------------------------------------
    mb = build_monument()
    p, kb, nf = save_prop(models, "monument", mb, "materials", PALETTE, assets_dir)
    made.append("monument.obj")
    placed.append(("Monument", [0, 0, 0], 0, ["poi"]))

    # --- scene, state LAST, log --------------------------------------------
    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, a.name)
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": a.name,
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"terrain": a.seed},
      "chunk_size": a.chunk_size, "radius": a.radius,
      "camera": {"target": [0, 1, 0], "distance": 24},
      # chunk verts are world-space; nodes/state sit at identity (audit 2.1)
      "chunks": [{"id": c, "path": "assets/" + r, "position": [0, 0, 0]}
                 for c, r in registry],
      "palette": PALETTE,
      "gameplay": GAMEPLAY,
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "%s custom world (seed %d)" % (a.name, a.seed),
               ["%d chunks, %d other assets" % (len(registry), len(made)-len(registry)),
                "replace PALETTE/build_monument/GAMEPLAY to make it yours"])
    print("[custom] ready: %d chunks + %d assets | %s" % (len(registry), len(made), a.name))

if __name__ == "__main__":
    main()