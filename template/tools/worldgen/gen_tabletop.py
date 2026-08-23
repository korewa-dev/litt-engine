#!/usr/bin/env python3
"""COUNCIL OF SIX - hex tabletop board generator (Litt Engine).

A 37-tile hex strategy board (pointy-top axial layout) with terrain bands,
player pawns, dice and a wooden frame. Genre math: axial hex coordinates
(x = s*sqrt(3)*(q + r/2), z = s*1.5*r), fBm terrain bands.
Usage: python gen_tabletop.py [--out-dir .] [--agent ai] [--prompt "..."]
"""
import argparse
import datetime
import math
from pathlib import Path

from worldkit import (Rng, fbm, MeshBuilder, write_mtl_for, register_index,
                      write_scene, write_state, append_log, save_prop)

SEED_T = 777; HEX = 0.95
MATS = {
  "board_wood": (0.45, 0.32, 0.20), "tile_water": (0.20, 0.42, 0.62),
  "tile_plains": (0.72, 0.66, 0.38), "tile_forest": (0.28, 0.48, 0.30),
  "tile_mountain": (0.52, 0.52, 0.54),
  "pawn_red": (0.75, 0.20, 0.18), "pawn_blue": (0.18, 0.35, 0.72),
  "pawn_green": (0.22, 0.60, 0.26), "pawn_gold": (0.85, 0.68, 0.20),
  "pawn_violet": (0.50, 0.25, 0.65), "pawn_black": (0.12, 0.12, 0.13),
  "die_white": (0.92, 0.92, 0.90)
}
PAWN_COLORS = ["pawn_red", "pawn_blue", "pawn_green", "pawn_gold", "pawn_violet", "pawn_black"]

class Kit:
    def __init__(self, mb): self.mb = mb
    def __call__(self, pname, mat):
        self.mb.begin(pname, mat)
        return PartHandle(self.mb)

class PartHandle:
    def __init__(self, mb): self.mb = mb
    def box(self, *a): self.mb.box(*a)
    def cyl(self, *a, **k): self.mb.cyl(*a, **k)
    def prism(self, *a): self.mb.roof_prism(*a)
    def hex_tile(self, *a, **k): self.mb.hex_tile(*a, **k)

def build(mb, fn):
    fn(Kit(mb))
    return mb

def tile_kind(q, r, seed):
    n = fbm(q * 0.5 + 9, r * 0.5 + 9, seed)
    if n < 0.34: return ("tile_water", 0.06)
    if n < 0.55: return ("tile_plains", 0.16)
    if n < 0.74: return ("tile_forest", 0.24)
    return ("tile_mountain", 0.36)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--seed", type=int, default=SEED_T)
    ap.add_argument("--agent", default="ai-agent")
    ap.add_argument("--prompt", default=None)
    a = ap.parse_args()

    root = Path(a.out_dir); models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    assets_dir = root / "assets"
    write_mtl_for(models, "materials", MATS)
    placed = []; made = []

    mb = MeshBuilder()
    kit = Kit(mb)
    frame = kit("frame", "board_wood")
    frame.box(0, -0.09, 0, 6.6, 0.08, 5.9)
    board_tiles = []
    for q in range(-3, 4):
        for r in range(-3, 4):
            if abs(q + r) > 3: continue
            x = HEX * math.sqrt(3) * (q + r / 2.0)
            z = HEX * 1.5 * r
            kind, h = tile_kind(q, r, a.seed)
            frame.hex_tile(x, 0, z, HEX * 0.96, h)
            board_tiles.append((x, z, kind))
    name = "hex_board"
    obj_text, nv, nf = mb.to_obj(name, "materials")
    (models / (name + ".obj")).write_text(obj_text, encoding="utf-8")
    register_index(assets_dir, name, "models/" + name + ".obj")
    made.append(name + ".obj")
    placed.append(("Hex_Board", [0, 0, 0], 0, ["board","tabletop"]))

    rng = Rng(a.seed + 1)
    corners = sorted(board_tiles, key=lambda t: -(abs(t[0]) + abs(t[1])))[:6]
    for i, (x, z, kind) in enumerate(corners):
        cname = "Pawn_%02d" % (i+1); cmat = PAWN_COLORS[i % len(PAWN_COLORS)]
        mb = MeshBuilder()
        pawn = kit2 = None
        k = Kit(mb); pw = k("pawn", cmat)
        pw.cyl(x, 0.1, z, 0.15, 0.11, 0.42, seg=8)
        head = k("head", cmat); head.cyl(x, 0.52, z, 0.10, 0.02, 0.10, seg=8)
        save_prop(models, cname, mb, "materials", MATS, assets_dir)
        made.append(cname + ".obj")
        placed.append((cname, [round(x,3), 0, round(z,3)], 0, ["token","player"]))

    for i in range(2):
        dname = "Die_%02d" % (i+1)
        mb = MeshBuilder(); k = Kit(mb); d = k("cube", "die_white")
        dx = -0.7 + i * 1.4
        d.box(dx, 0.26, 0.0, 0.16, 0.16, 0.16)
        save_prop(models, dname, mb, "materials", MATS, assets_dir)
        made.append(dname + ".obj")
        placed.append((dname, [dx, 0, 0], int(rng.uniform(0, 90)), ["dice"]))

    write_scene(root / "assets" / "scenes" / "world.lscn.json", placed, "council-of-six")
    state = {
      "format": "litt-live-state", "version": 1, "mode": "ai-exclusive",
      "theme": "council-of-six",
      "updated": datetime.datetime.now().isoformat(timespec="seconds"),
      "seed": {"terrain": a.seed},
      "chunk_size": 0, "radius": 0,
      "camera": {"target": [0, 0, 0], "distance": 24},
      "chunks": [],
      "palette": MATS,
      "gameplay": {"genre": "tabletop_strategy",
                   "players": "2-6 (six pawn colors)",
                   "turn_structure": "roll d6 -> move -> resolve tile",
                   "tiles": {"water": "blocked", "plains": "1 move", "forest": "2 moves", "mountain": "3 moves"},
                   "win_condition": "first to reach the opposite edge tile"}
    }
    write_state(root / "world_state.json", state)
    append_log(root / "LIVE_LOG.md", a.agent, a.prompt,
               "COUNCIL OF SIX hex tabletop board (seed %d)" % a.seed,
               ["37 hex tiles banded by fBm, 6 pawns, 2 dice, wooden frame",
                "turn rules written into world_state.json"])
    print("[tabletop] ready: %d assets, %d scene nodes" % (len(made), len(placed)))

if __name__ == "__main__":
    main()