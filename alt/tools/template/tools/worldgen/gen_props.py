#!/usr/bin/env python3
"""gen_props.py - emit the standard gameplay prop kit into a generated world.

Runtimes instantiate only nodes carrying `model:<name>` tags, and enrichment
(enrich_game.py) wants named enemies/pickups/checkpoints to place. This tool
emits the shared prop library so every game speaks the same vocabulary:

  survivor kit : coin, gem, heart, brazier, wraith, brute, spike
  platformer kit: coin, gem, checkpoint_flag, drone, spike, banner
  souls kit    : coin, gem, estus_flask, bonfire, stalker, knight, banner
  shared kit v2: cross-genre pieces absorbed FROM generator bespoke meshes
                 (ASSET_AUDIT fix 1.1) so generators can migrate onto kit
                 refs drop-in: goal_gate, fog_veil, hazard_pit,
                 hazard_spikes, hex_pawn, token_gem, star_glint,
                 asteroid_small/medium/large, platform_deck_short/mid/long,
                 ruin_arch, ruin_pillar

Kit hygiene (fix 1.4 / audit item 2.4): EVERY piece - legacy and v2 - is
re-centered through worldkit.recenter_mesh so the x/z vertex centroid sits
on origin well inside the 0.05 m tolerance. Consumers should call
save_prop(..., enforce_origin=True) and retire auto_recenter.

Determinism (fix 1.2): thread rng=worldkit.Rng(seed) into build_prop for
deterministic per-piece variation (whole-mesh axis scales held inside
silhouette bounds). rng=None emits the exact unvaried geometry, run after
run - byte-stable. The --seed CLI flag derives one stable sub-seed per
piece name, so piece order never matters.

Palettes (fix 1.3): PALETTES is DERIVED from themes.json - one theme
vocabulary shared with the generators (worldkit.load_theme consumers).
Each canonical prop key (metal, metal_dk, gold, blood, ember, bone, void,
glow, hide, wood, stone, rock) aliases onto whatever that theme actually
names its colors, with neutral fallbacks. Legacy keys dark_fantasy /
cyberpunk_neon / haunted_estate resolve exactly as before because those
themes exist in themes.json.

Usage:
  python gen_props.py --game-dir Project/kingsfall-hollow --kit souls [--seed N]
"""
import argparse
import json
import zlib
from pathlib import Path

from worldkit import (MeshBuilder, Rng, save_prop, write_mtl_for,
                      register_index, recenter_mesh)

# ------------------------------------------------------- themes.json palettes
# Canonical prop vocabulary -> candidate key names inside a theme palette
# (first hit wins; otherwise the neutral fallback below). Candidate lists are
# ordered most-literal-first so themes that speak our nouns get exact hits.
_CANON_ALIASES = {
    "metal": ("iron", "machinery", "bronze", "panel_grey", "hull_frame",
              "dead_metal", "radio_grey", "granite_grey", "concrete"),
    "metal_dk": ("rivet_dark", "iron_banded", "dark_glass", "charred",
                 "volcano_dark", "building_black", "canopy_dark",
                 "pitch_black", "road_dark"),
    "gold": ("gold_trim", "sheriff_star_gold", "gold_leaf", "ore_vein_gold",
             "brass", "holo_yellow", "dune_gold", "streetlight_amber"),
    "blood": ("blood", "hazard_red", "signage_red", "warning_red",
              "mushroom_red", "block_red", "neon_pink", "rust_orange"),
    "ember": ("ember_orange", "coral_orange", "rust_orange",
              "streetlight_amber", "lit_window", "candle_glow",
              "warning_stripe"),
    "bone": ("white_stone", "marble_white", "snow_white", "sugar_white",
             "sand_white", "cloud_white", "spectre_pale", "plaster",
             "willow_pale", "steam_white", "hull_white"),
    "void": ("grave_night", "building_black", "pitch_black", "deep_blue",
             "wet_asphalt", "asphalt", "charred", "canopy_dark"),
    "glow": ("neon_cyan", "neon_pink", "holo_yellow", "candle_glow",
             "lit_window", "aurora_green", "crystal_amethyst",
             "window_blue", "fungus_teal"),
    "hide": ("leather_brown", "camo_brown", "bark_brown", "peat_brown",
             "adobe", "dust_tan", "sandbag_tan", "terracotta", "choc"),
    "wood": ("rot_wood", "dark_wood", "timber", "sunbleached_wood",
             "bark_brown", "bridge_rope"),
    "stone": ("stone", "cave_stone", "limestone", "ruin_sandstone",
              "manor_grey", "concrete", "pack_ice_grey", "sidewalk_grey",
              "baseplate_grey"),
    "rock": ("granite_grey", "cave_stone", "charred", "volcano_dark",
             "ruin_sandstone", "peat_brown"),
}
_CANON_FALLBACK = {
    "metal": (0.40, 0.38, 0.34), "metal_dk": (0.22, 0.21, 0.19),
    "gold": (0.90, 0.75, 0.30), "blood": (0.55, 0.10, 0.12),
    "ember": (1.00, 0.60, 0.20), "bone": (0.88, 0.86, 0.78),
    "void": (0.12, 0.12, 0.16), "glow": (0.95, 0.85, 0.45),
    "hide": (0.32, 0.27, 0.22), "wood": (0.28, 0.21, 0.14),
    "stone": (0.44, 0.45, 0.46), "rock": (0.36, 0.35, 0.34),
}


def _derive_palette(theme_pal):
    """Map one theme's own palette onto the canonical prop vocabulary."""
    out = {}
    for canon, candidates in _CANON_ALIASES.items():
        hit = next((tuple(theme_pal[c]) for c in candidates
                    if c in theme_pal), None)
        out[canon] = hit or _CANON_FALLBACK[canon]
    return out


def _load_palettes():
    """PALETTES from themes.json: {theme_name: {canon_key: (r,g,b)}}."""
    p = Path(__file__).resolve().parent / "themes.json"
    data = json.loads(p.read_text(encoding="utf-8"))
    return {name: _derive_palette(theme.get("palette", {}))
            for name, theme in data.get("themes", {}).items()}


PALETTES = _load_palettes()

# Deterministic per-piece variation, fix 1.2: independent axis scales held
# inside +-6% / +-5% so silhouettes stay within tolerance, then an x/z
# re-center so the origin convention survives variation.
_SCALE_XZ = 0.12   # total span around 1.0 -> 0.94 .. 1.06
_SCALE_Y = 0.10    # -> 0.95 .. 1.05


def _apply_variation(mb, rng):
    sx = 1.0 - _SCALE_XZ / 2.0 + _SCALE_XZ * rng.uniform()
    sz = 1.0 - _SCALE_XZ / 2.0 + _SCALE_XZ * rng.uniform()
    sy = 1.0 - _SCALE_Y / 2.0 + _SCALE_Y * rng.uniform()
    for p in mb.v:
        p[0] *= sx
        p[1] *= sy
        p[2] *= sz


def _mb():
    return MeshBuilder()


def build_prop(name, pal=None, rng=None):
    """Return a MeshBuilder holding the named prop, or None.

    `pal` is accepted for call compatibility (gen_soulslike passes the
    prefixed palette) but unused: part materials are the hardcoded
    `prop_<key>` names that the caller merges into materials.mtl.
    `rng` (a worldkit.Rng) drives deterministic per-piece variation; None
    means the exact unvaried geometry."""
    def pal_get(k):
        return 'prop_' + k
    mb = _mb()

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

    # ---- shared kit v2: absorbed generator bespoke meshes (fix 1.1) ----
    # Each re-created AT ORIGIN within silhouette tolerance of the
    # generator original it replaces, so future migration is drop-in.
    elif name == "goal_gate":
        # goal/jump gate: twin pylons + beacon bar (gen_space p_gate)
        mb.begin("gate_pylons", pal_get("metal"))
        mb.cyl(-1.3, 0, 0, 0.10, 0.16, 2.6, seg=8)
        mb.cyl(1.3, 0, 0, 0.10, 0.16, 2.6, seg=8)
        mb.begin("gate_beacon", pal_get("glow"))
        mb.box(0, 2.72, 0, 2.9, 0.16, 0.16)
    elif name == "fog_veil":
        # boss-approach fog wall (gen_soulslike p_fog_gate)
        mb.begin("fog_veil", pal_get("bone"))
        mb.box(0, 1.55, 0, 3.1, 1.55, 0.14)
    elif name == "hazard_pit":
        # pit marker slab; scene node carries the y=-1.5 ride height
        # (gen_platformer25d hazard_pit)
        mb.begin("hazard_pit", pal_get("void"))
        mb.box(0, -0.5, 0, 1.4, 0.2, 1.3)
    elif name == "hazard_spikes":
        # symmetric spike cluster (gen_platformer25d hazard_spikes)
        mb.begin("hazard_spikes", pal_get("metal_dk"))
        for sxp in (-1.05, -0.35, 0.35, 1.05):
            mb.cone(sxp, -0.35, 0, 0.16, 0.5, seg=6)
    elif name == "hex_pawn":
        # tabletop pawn: tapered body + head disc (gen_tabletop Pawn_XX)
        mb.begin("pawn_body", pal_get("gold"))
        mb.cyl(0, 0.1, 0, 0.15, 0.11, 0.42, seg=8)
        mb.begin("pawn_head", pal_get("gold"))
        mb.cyl(0, 0.52, 0, 0.10, 0.02, 0.10, seg=8)
    elif name == "token_gem":
        # pickup gem instanced on board tiles (gen_tabletop token_gem)
        mb.begin("token_gem", pal_get("gold"))
        mb.octahedron(0, 0.16, 0, 0.16)
    elif name == "star_glint":
        # backdrop star glint, instanced N times (gen_space p_star)
        mb.begin("star_glint", pal_get("glow"))
        mb.octahedron(0, 0.12, 0, 0.14)
    elif name in ("asteroid_small", "asteroid_medium", "asteroid_large"):
        # drifting rock: core box + offset lump (gen_space p_asteroid
        # silhouette with the random width frozen to three sizes)
        w = {"asteroid_small": 0.7, "asteroid_medium": 1.1,
             "asteroid_large": 1.6}[name]
        mb.begin("asteroid_core", pal_get("rock"))
        mb.box(0, w * 0.6, 0, w / 2, w * 0.45, w * 0.4)
        mb.begin("asteroid_lump", pal_get("stone"))
        mb.box(w * 0.3, w * 0.9, w * 0.2, w * 0.28, w * 0.25, w * 0.25)
    elif name in ("platform_deck_short", "platform_deck_mid",
                  "platform_deck_long"):
        # walkable deck slab, centered slab-style like the 25d originals
        # (gen_platformer25d PLATFORM_VARIANTS half-widths 1.0/1.3/1.6)
        hw = {"short": 1.0, "mid": 1.3, "long": 1.6}[name.rsplit("_", 1)[1]]
        mb.begin("deck", pal_get("metal"))
        mb.box(0, 0, 0, hw, 0.12, 1.1)
    elif name == "ruin_arch":
        # broken gateway: two jambs + lintel (gen_soulslike p_arch)
        mb.begin("arch_stone", pal_get("stone"))
        mb.box(-2.6, 1.9, 0, 0.45, 1.9, 0.45)
        mb.box(2.6, 1.9, 0, 0.45, 1.9, 0.45)
        mb.box(0, 4.0, 0, 3.05, 0.30, 0.50)
    elif name == "ruin_pillar":
        # fallen-column stump at the ref's mid-range height
        # (gen_soulslike p_pillar rolls 1.6..3.4)
        mb.begin("pillar_stone", pal_get("stone"))
        mb.cyl(0, 0, 0, 0.38, 0.32, 2.4, seg=9)
    else:
        return None

    if rng is not None:
        _apply_variation(mb, rng)
    # Fix 1.4: every kit piece ships origin-clean (x/z centroid on 0), so
    # consumers can enforce_origin instead of auto_recenter (the audit's
    # bonfire z=-0.06 complaint).
    recenter_mesh(mb)
    return mb


KITS = {
    "survivor": ["coin", "gem", "heart", "brazier", "wraith", "brute", "spike"],
    "platformer": ["coin", "gem", "checkpoint_flag", "drone", "spike", "banner"],
    "souls": ["coin", "gem", "estus_flask", "bonfire", "stalker", "knight", "banner"],
    "shared": ["goal_gate", "fog_veil", "hazard_pit", "hazard_spikes",
               "hex_pawn", "token_gem", "star_glint",
               "asteroid_small", "asteroid_medium", "asteroid_large",
               "platform_deck_short", "platform_deck_mid",
               "platform_deck_long", "ruin_arch", "ruin_pillar"],
}

# Kit-default theme keys (themes.json names). Legacy three unchanged.
KIT_THEMES = {
    "survivor": "dark_fantasy", "platformer": "cyberpunk_neon",
    "souls": "haunted_estate", "shared": "dark_fantasy",
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


def piece_seed(seed, name):
    """Stable per-piece sub-seed: piece order never matters."""
    return ((seed * 0x9E3779B9) + zlib.crc32(name.encode("utf-8"))) & 0xFFFFFFFF


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--kit", required=True, choices=sorted(KITS))
    ap.add_argument("--theme-palette", default=None,
                    help="theme name inside themes.json "
                         "(defaults to the kit-appropriate theme)")
    ap.add_argument("--seed", type=int, default=None,
                    help="seed deterministic per-piece variation (sizes "
                         "jitter within silhouette bounds); omit for the "
                         "exact unvaried geometry")
    ap.add_argument("--force", action="store_true",
                    help="re-emit props even if the .obj already exists")
    a = ap.parse_args()

    root = Path(a.game_dir)
    models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    pal_key = a.theme_palette or KIT_THEMES[a.kit]
    if pal_key not in PALETTES:
        raise SystemExit("unknown theme %r - available: %s"
                         % (pal_key, ", ".join(sorted(PALETTES))))
    pal = PALETTES[pal_key]

    # MERGE with the world's existing palette - never recolor built meshes.
    merged = parse_mtl(models / "materials.mtl")
    for k, v in pal.items():
        merged.setdefault("prop_" + k, v)
    write_mtl_for(models, "materials", merged)

    made = []
    for name in KITS[a.kit]:
        target = models / (name + ".obj")
        if target.exists() and not a.force:
            made.append(name + "(kept)")
            continue
        rng = None if a.seed is None else Rng(piece_seed(a.seed, name))
        mb = build_prop(name, None, rng)
        if mb is None:
            continue
        _, kb, tris = save_prop(models, name, mb, "materials", merged,
                                assets_dir=root / "assets",
                                enforce_origin=True)
        made.append("%s(%dkb,%dtris)" % (name, int(kb) + 1, tris))

    suffix = "" if a.seed is None else " seed=%d" % a.seed
    print("[props] %s <- %s kit%s: %s"
          % (root.name, a.kit, suffix, ", ".join(made)))


if __name__ == "__main__":
    main()
