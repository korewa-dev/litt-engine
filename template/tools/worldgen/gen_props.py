#!/usr/bin/env python3
"""gen_props.py - emit the standard gameplay prop kit into a generated world.

Runtimes instantiate only nodes carrying `model:<name>` tags, and enrichment
(enrich_game.py) wants named enemies/pickups/checkpoints to place. This tool
emits the shared prop library so every game speaks the same vocabulary:

  survivor kit : coin, gem, heart, brazier, wraith, brute, spike
  platformer kit: coin, gem, checkpoint_flag, drone, spike, goal_banner
  souls kit    : coin, gem, estus_flask, bonfire, stalker, knight, banner

Usage:
  python gen_props.py --game-dir Project/kingsfall-hollow --kit souls [--seed N]
"""
import argparse
import math
from pathlib import Path

from worldkit import (MeshBuilder, save_prop, write_mtl_for,
                      register_index)

PALETTES = {
    "dark_fantasy": {"metal": (0.45, 0.30, 0.18), "gold": (0.95, 0.72, 0.15),
                     "blood": (0.70, 0.12, 0.10), "ember": (1.00, 0.45, 0.10),
                     "bone": (0.85, 0.82, 0.72), "void": (0.16, 0.12, 0.20),
                     "metal_dk": (0.24, 0.16, 0.11), "glow": (1.00, 0.80, 0.30),
                     "hide": (0.42, 0.28, 0.18), "wood": (0.36, 0.24, 0.13)},
    "cyberpunk_neon": {"metal": (0.25, 0.28, 0.34), "gold": (1.00, 0.84, 0.20),
                       "blood": (1.00, 0.15, 0.35), "ember": (0.10, 0.95, 0.85),
                       "bone": (0.80, 0.85, 1.00), "void": (0.08, 0.06, 0.14),
                       "metal_dk": (0.13, 0.15, 0.19), "glow": (0.20, 1.00, 0.90),
                       "hide": (0.30, 0.33, 0.40), "wood": (0.20, 0.22, 0.28)},
    "haunted_estate": {"metal": (0.40, 0.38, 0.34), "gold": (0.90, 0.75, 0.30),
                       "blood": (0.55, 0.10, 0.12), "ember": (1.00, 0.60, 0.20),
                       "bone": (0.88, 0.86, 0.78), "void": (0.12, 0.12, 0.16),
                       "metal_dk": (0.22, 0.21, 0.19), "glow": (0.95, 0.85, 0.45),
                       "hide": (0.32, 0.27, 0.22), "wood": (0.28, 0.21, 0.14)},
}


def _mb():
    return MeshBuilder()


def build_prop(name, pal):
    def pal_get(k):
        return 'prop_' + k
    """Return a MeshBuilder holding the named prop, or None."""
    mb = _mb()

    def part(mat):
        # single-material helper bound to this builder
        return mat

    if name == "coin":
        mb.begin("coin", pal_get("gold"))
        mb.cyl(0, 0.5, 0, 0.32, 0.32, 0.07, seg=12)
    elif name == "gem":
        mb.begin("gem", pal_get("ember"))
        mb.octahedron(0, 0.55, 0, 0.30)
    elif name == "heart":
        mb.begin("heart", pal_get("blood"))
        mb.box(0, 0.55, 0, 0.42, 0.30, 0.22)
        mb.pyramid(0, 0.25, 0, 0.42, 0.42, 0.35)
    elif name == "brazier":
        mb.begin("brazier", pal_get("void"))
        mb.cyl(0, 0.0, 0, 0.42, 0.30, 1.05, seg=8)
        mb.begin("brazier_flame", pal_get("ember"))
        mb.cone(0, 1.05, 0, 0.30, 0.65, seg=7)
    elif name == "checkpoint_flag":
        mb.begin("flag_pole", pal_get("bone"))
        mb.cyl(0, 0.0, 0, 0.06, 0.05, 2.6, seg=6)
        mb.begin("flag_cloth", pal_get("ember"))
        mb.roof_prism(0.45, 2.0, 0.0, 0.45, 0.05, 0.55)
    elif name == "drone":
        # quad-rotor: hull + canopy + 4 arms + 4 independent rotors
        mb.begin("drone_hull", pal_get("metal"))
        mb.box(0, 1.6, 0, 0.22, 0.09, 0.22)
        mb.begin("drone_canopy", pal_get("glow"))
        mb.sphere(0, 1.73, 0, 0.12, seg=8, rings=5)
        mb.begin("drone_eye", pal_get("blood"))
        mb.octahedron(0, 1.60, -0.26, 0.10)
        mb.begin("drone_arm_x", pal_get("metal_dk"))
        mb.box(0, 1.66, 0, 0.48, 0.03, 0.05)
        mb.begin("drone_arm_z", pal_get("metal_dk"))
        mb.box(0, 1.66, 0, 0.05, 0.03, 0.48)
        for tag, px, pz in (("rotor_n", 0, -0.42), ("rotor_e", 0.42, 0),
                            ("rotor_s", 0, 0.42), ("rotor_w", -0.42, 0)):
            mb.begin("drone_" + tag, pal_get("metal_dk"))
            mb.cyl(px, 1.70, pz, 0.16, 0.16, 0.02, seg=8)
    elif name == "spike":
        mb.begin("spikes", pal_get("metal_dk"))
        for i in range(3):
            x = -0.35 + 0.35 * i
            mb.cone(x, 0.0, 0.0, 0.13, 0.62, seg=5)
    elif name == "wraith":
        # floating shroud with sleeves and a swaying lantern
        mb.begin("wraith_shroud", pal_get("void"))
        mb.cyl(0, 0.0, 0, 0.30, 0.52, 1.7, seg=8)
        mb.begin("wraith_mask", pal_get("bone"))
        mb.sphere(0, 1.58, 0, 0.20, seg=8, rings=5)
        mb.begin("wraith_eye_l", pal_get("glow"))
        mb.sphere(-0.08, 1.62, -0.15, 0.045, seg=6, rings=4)
        mb.begin("wraith_eye_r", pal_get("glow"))
        mb.sphere(0.08, 1.62, -0.15, 0.045, seg=6, rings=4)
        mb.begin("wraith_sleeve_l", pal_get("void"))
        mb.cyl(-0.42, 1.25, 0, 0.07, 0.04, 0.55, seg=6)
        mb.begin("wraith_sleeve_r", pal_get("void"))
        mb.cyl(0.42, 1.25, 0, 0.07, 0.04, 0.55, seg=6)
        mb.begin("wraith_lantern", pal_get("ember"))
        mb.sphere(-0.55, 0.95, 0.05, 0.09, seg=6, rings=4)
    elif name == "brute":
        # full walker: torso, head, horns, two arms, two legs
        mb.begin("brute_torso", pal_get("blood"))
        mb.box(0, 1.15, 0, 0.31, 0.65, 0.28)
        mb.begin("brute_head", pal_get("void"))
        mb.sphere(0, 2.02, 0, 0.20, seg=8, rings=5)
        mb.begin("brute_horn_l", pal_get("bone"))
        mb.cone(-0.17, 2.10, 0, 0.05, 0.24, seg=5)
        mb.begin("brute_horn_r", pal_get("bone"))
        mb.cone(0.17, 2.10, 0, 0.05, 0.24, seg=5)
        mb.begin("brute_arm_l", pal_get("hide"))
        mb.box(-0.42, 1.28, 0, 0.08, 0.44, 0.08)
        mb.begin("brute_arm_r", pal_get("hide"))
        mb.box(0.42, 1.28, 0, 0.08, 0.44, 0.08)
        mb.begin("brute_leg_l", pal_get("void"))
        mb.box(-0.18, 0.40, 0, 0.11, 0.42, 0.13)
        mb.begin("brute_leg_r", pal_get("void"))
        mb.box(0.18, 0.40, 0, 0.11, 0.42, 0.13)
    elif name == "bonfire":
        mb.begin("bonfire_stack", pal_get("metal_dk"))
        mb.cyl(0, 0.0, 0, 0.55, 0.40, 0.30, seg=9)
        mb.begin("bonfire_logs", pal_get("wood"))
        mb.box(0.22, 0.34, 0.10, 0.42, 0.05, 0.06)
        mb.box(-0.20, 0.36, -0.08, 0.40, 0.05, 0.06)
        mb.begin("bonfire_flame", pal_get("ember"))
        mb.cone(0, 0.38, 0, 0.36, 1.00, seg=8)
        mb.begin("bonfire_spark_a", pal_get("glow"))
        mb.sphere(0.12, 0.95, 0.06, 0.05, seg=6, rings=4)
        mb.begin("bonfire_spark_b", pal_get("glow"))
        mb.sphere(-0.14, 1.15, -0.04, 0.04, seg=6, rings=4)
    elif name == "stalker":
        mb.begin("stalker_body", pal_get("void"))
        mb.cyl(0, 0.2, 0, 0.18, 0.34, 1.5, seg=8)
        mb.begin("stalker_hood", pal_get("blood"))
        mb.pyramid(0, 1.7, 0, 0.34, 0.34, 0.55)
        mb.begin("stalker_eye", pal_get("glow"))
        mb.sphere(0, 1.52, -0.16, 0.07, seg=6, rings=4)
        mb.begin("stalker_blade_l", pal_get("metal_dk"))
        mb.box(-0.44, 1.05, 0, 0.04, 0.50, 0.09)
        mb.begin("stalker_blade_r", pal_get("metal_dk"))
        mb.box(0.44, 1.05, 0, 0.04, 0.50, 0.09)
    elif name == "knight":
        mb.begin("knight_torso", pal_get("metal"))
        mb.box(0, 1.15, 0, 0.28, 0.60, 0.21)
        mb.begin("knight_head", pal_get("metal"))
        mb.sphere(0, 1.98, 0, 0.17, seg=8, rings=5)
        mb.begin("knight_plume", pal_get("blood"))
        mb.pyramid(0, 2.18, -0.03, 0.11, 0.11, 0.34)
        mb.begin("knight_arm_l", pal_get("metal"))
        mb.box(-0.36, 1.28, 0, 0.07, 0.40, 0.07)
        mb.begin("knight_arm_r", pal_get("metal"))
        mb.box(0.36, 1.28, 0, 0.07, 0.40, 0.07)
        mb.begin("knight_sword", pal_get("gold"))
        mb.box(0.46, 0.72, 0.02, 0.035, 0.48, 0.05)
        mb.begin("knight_leg_l", pal_get("metal_dk"))
        mb.box(-0.14, 0.42, 0, 0.09, 0.44, 0.10)
        mb.begin("knight_leg_r", pal_get("metal_dk"))
        mb.box(0.14, 0.42, 0, 0.09, 0.44, 0.10)
    elif name == "banner":
        mb.begin("banner_pole", pal_get("metal"))
        mb.cyl(0, 0.0, 0, 0.09, 0.07, 4.6, seg=8)
        mb.begin("banner_cloth", pal_get("blood"))
        mb.roof_prism(0.0, 3.3, 0.75, 0.06, 0.7, 1.1)
    elif name == "estus_flask":
        mb.begin("estus_glass", pal_get("ember"))
        mb.cyl(0, 0.25, 0, 0.16, 0.20, 0.5, seg=9)
        mb.begin("estus_cap", pal_get("metal_dk"))
        mb.cyl(0, 0.75, 0, 0.09, 0.09, 0.12, seg=6)
        mb.begin("estus_glow", pal_get("glow"))
        mb.sphere(0, 0.52, 0, 0.09, seg=6, rings=4)
    else:
        return None
    return mb


KITS = {
    "survivor": ["coin", "gem", "heart", "brazier", "wraith", "brute", "spike"],
    "platformer": ["coin", "gem", "checkpoint_flag", "drone", "spike", "banner"],
    "souls": ["coin", "gem", "estus_flask", "bonfire", "stalker", "knight", "banner"],
}


def parse_mtl(path: Path):
    """Read an existing materials.mtl into {name: (r,g,b)} (Kd only)."""
    out = {}
    if not path.exists():
        return out
    cur = None
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("newmtl "):
            cur = line[7:].strip()
        elif line.startswith("Kd ") and cur:
            bits = line.split()
            try:
                out[cur] = tuple(float(x) for x in bits[1:4])
            except ValueError:
                pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--kit", required=True, choices=sorted(KITS))
    ap.add_argument("--theme-palette", default=None,
                    help="key inside PALETTES (defaults to kit-appropriate)")
    ap.add_argument("--force", action="store_true",
                    help="re-emit props even if the .obj already exists")
    a = ap.parse_args()

    root = Path(a.game_dir)
    models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    pal_key = a.theme_palette or {
        "survivor": "dark_fantasy", "platformer": "cyberpunk_neon",
        "souls": "haunted_estate"}[a.kit]
    pal = PALETTES[pal_key]

    # MERGE with the world's existing palette - never recolor built meshes.
    merged = parse_mtl(models / "materials.mtl")
    for k, v in pal.items():
        merged.setdefault("prop_" + k, v)
    write_mtl_for(models, "materials", merged)

    # Prop parts must reference the prefixed prop_* materials.
    ppal = {"prop_" + k: v for k, v in pal.items()}

    made = []
    for name in KITS[a.kit]:
        target = models / (name + ".obj")
        if target.exists() and not a.force:
            made.append(name + "(kept)")
            continue
        mb = build_prop(name, ppal)
        if mb is None:
            continue
        _, kb, tris = save_prop(models, name, mb, "materials", merged,
                                assets_dir=root / "assets")
        made.append("%s(%dkb,%dtris)" % (name, int(kb) + 1, tris))

    print("[props] %s <- %s kit: %s" % (root.name, a.kit, ", ".join(made)))


if __name__ == "__main__":
    main()

