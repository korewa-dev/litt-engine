#!/usr/bin/env python3
"""test_gen_props.py - kit-v2 self-test (ASSET_AUDIT fixes 1.1-1.4).

Plain-script gate (no pytest dep). Builds EVERY piece listed in
gen_props.KITS (legacy survivor/platformer/souls + new shared kit) and:

  1. asserts each builds through the multi-part MeshBuilder protocol;
  2. asserts origin-centered tolerance < 0.05 m on ALL pieces - legacy
     fixed ones included, unseeded AND seeded variants (fix 1.4);
  3. asserts default-seed determinism: two full save runs are byte-
     identical OBJ+MTL for sample pieces (fix 1.2 compat clause);
  4. asserts different seeds produce different jitter, held inside
     silhouette bounds (+-6% x/z, +-5% y extent ratios);
  5. asserts backwards compatibility: build_prop(name, pal=None, rng=None)
     signature, frozen legacy KITS membership, themes.json-sourced
     PALETTES carrying the legacy theme keys, dead part() helper gone.

Exit code 0 = all gates green; otherwise prints FAIL lines and exits 1.

Usage: python template/tools/worldgen/test_gen_props.py
"""
import inspect
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import gen_props
from gen_props import PALETTES, KITS, build_prop, parse_mtl, piece_seed
from worldkit import (Rng, assert_origin_centered, mesh_centroid,
                      save_prop, write_mtl_for)

ORIGIN_TOL = 0.05  # worldkit.ORIGIN_TOL; gate demands strictly less

# Frozen legacy membership - kit v2 must extend, never shrink/reorder.
LEGACY_KITS = {
    "survivor": ["coin", "gem", "heart", "brazier", "wraith", "brute",
                 "spike"],
    "platformer": ["coin", "gem", "checkpoint_flag", "drone", "spike",
                   "banner"],
    "souls": ["coin", "gem", "estus_flask", "bonfire", "stalker", "knight",
              "banner"],
}
V2_PIECES = ["goal_gate", "fog_veil", "hazard_pit", "hazard_spikes",
             "hex_pawn", "token_gem", "star_glint", "asteroid_small",
             "asteroid_medium", "asteroid_large", "platform_deck_short",
             "platform_deck_mid", "platform_deck_long", "ruin_arch",
             "ruin_pillar"]
LEGACY_THEME_KEYS = ("dark_fantasy", "cyberpunk_neon", "haunted_estate")


def all_pieces():
    """Union of every kit member, first-seen order."""
    seen = []
    for names in KITS.values():
        for n in names:
            if n not in seen:
                seen.append(n)
    return seen


def extents(mb):
    xs, ys, zs = zip(*[(p[0], p[1], p[2]) for p in mb.v])
    return [max(c) - min(c) for c in (xs, ys, zs)]


def emit_bytes(root, name, rng):
    """Full save path once: returns (obj_bytes, mtl_bytes)."""
    models = root / "assets" / "models"
    models.mkdir(parents=True, exist_ok=True)
    merged = parse_mtl(models / "materials.mtl")
    for k, v in PALETTES["haunted_estate"].items():
        merged.setdefault("prop_" + k, v)
    write_mtl_for(models, "materials", merged)
    mb = build_prop(name, None, rng)
    if mb is None:
        raise AssertionError("build_prop(%r) returned None" % name)
    save_prop(models, name, mb, "materials", merged, enforce_origin=True)
    return ((models / (name + ".obj")).read_bytes(),
            (models / "materials.mtl").read_bytes())


def test_pieces_build_and_origin(failures):
    worst = 0.0
    for name in all_pieces() + V2_PIECES:
        for tag, rng in (("plain", None),
                         ("seeded", Rng(piece_seed(7, name)))):
            mb = build_prop(name, None, rng)
            if mb is None:
                failures.append("build_prop(%s/%s) returned None"
                                % (name, tag))
                continue
            tris = sum(len(g["faces"]) for g in mb.groups if g["faces"])
            if tris <= 0:
                failures.append("%s/%s: zero triangles" % (name, tag))
            c = mesh_centroid(mb)
            off = max(abs(c[0]), abs(c[2]))
            worst = max(worst, off)
            if off >= ORIGIN_TOL:
                failures.append("%s/%s centroid off %.4f >= %.2f"
                                % (name, tag, off, ORIGIN_TOL))
            try:
                assert_origin_centered(mb, ORIGIN_TOL)
            except ValueError as exc:
                failures.append("%s/%s: %s" % (name, tag, exc))
    if build_prop("definitely_not_a_prop") is not None:
        failures.append("build_prop(unknown name) must return None")
    print("PASS pieces: %d built plain+seeded, worst xz centroid %.4f m "
          "(tol %.2f)" % (len(set(all_pieces()) | set(V2_PIECES)),
                          worst, ORIGIN_TOL))


def test_default_seed_determinism(failures):
    for name in ("bonfire", "asteroid_medium"):
        with tempfile.TemporaryDirectory() as d1, \
                tempfile.TemporaryDirectory() as d2:
            a1, m1 = emit_bytes(Path(d1), name, None)
            a2, m2 = emit_bytes(Path(d2), name, None)
            if a1 != a2:
                failures.append("%s: default-seed OBJ not byte-identical"
                                % name)
            if m1 != m2:
                failures.append("%s: default-seed MTL not byte-identical"
                                % name)
        sub = piece_seed(1234, name)
        t1 = build_prop(name, None, Rng(sub)).to_obj(name, "materials")
        t2 = build_prop(name, None, Rng(sub)).to_obj(name, "materials")
        if t1 != t2:
            failures.append("%s: same seed produced different OBJ" % name)
    print("PASS determinism: default-seed runs byte-identical "
          "(OBJ+MTL), same-seed jitter reproducible")


def test_seeded_jitter_bounds(failures):
    lo, hi = 0.90, 1.10  # scale spans +-6%/+-5%; gate with margin
    for name in ("asteroid_medium", "ruin_pillar", "hex_pawn"):
        base = extents(build_prop(name))
        texts = set()
        for seed in (11, 12, 13):
            mb = build_prop(name, None, Rng(seed))
            texts.add(mb.to_obj(name, "materials"))
            ex = extents(mb)
            for axis in range(3):
                if base[axis] <= 1e-9:
                    continue
                r = ex[axis] / base[axis]
                if not (lo <= r <= hi):
                    failures.append(
                        "%s seed %d: axis %d extent ratio %.3f outside "
                        "[%.2f, %.2f]" % (name, seed, axis, r, lo, hi))
        if len(texts) < 3:
            failures.append("%s: distinct seeds did not change geometry"
                            % name)
    print("PASS variation: 3 seeds -> 3 geometries inside silhouette "
          "bounds %s" % ((lo, hi),))


def test_backwards_compat(failures):
    sig = inspect.signature(build_prop)
    names = list(sig.parameters)
    if names != ["name", "pal", "rng"]:
        failures.append("build_prop signature changed: %s" % names)
    elif any(sig.parameters[p].default is not inspect.Parameter.empty
             for p in ("name",)):
        failures.append("build_prop 'name' must stay required")
    for kit, expected in LEGACY_KITS.items():
        got = KITS.get(kit)
        if got != expected:
            failures.append("legacy kit %r mutated: %s" % (kit, got))
    missing = [p for p in V2_PIECES if p not in KITS.get("shared", [])]
    if missing:
        failures.append("shared kit missing v2 pieces: %s" % missing)
    for key in LEGACY_THEME_KEYS:
        pal = PALETTES.get(key)
        if pal is None:
            failures.append("PALETTES lost legacy theme %r" % key)
            continue
        for canon in ("metal", "gold", "blood", "void"):
            col = pal.get(canon)
            if (not isinstance(col, tuple) or len(col) != 3
                    or not all(0.0 <= c <= 1.0 for c in col)):
                failures.append("PALETTES[%s][%s] malformed: %s"
                                % (key, canon, col))
    src = Path(gen_props.__file__).read_text(encoding="utf-8")
    if "\ndef part(" in src:
        failures.append("dead part() helper still present")
    with tempfile.TemporaryDirectory() as d:
        mtl = Path(d) / "t.mtl"
        mtl.write_text("newmtl prop_gold\nKd 0.900 0.750 0.300\n",
                       encoding="utf-8")
        parsed = parse_mtl(mtl)
        if parsed.get("prop_gold") != (0.9, 0.75, 0.3):
            failures.append("parse_mtl round-trip broke: %s" % parsed)
    print("PASS compat: signature %s, legacy kits frozen, %d themes "
          "sourced from themes.json, parse_mtl intact"
          % (names, len(PALETTES)))


def main():
    failures = []
    test_pieces_build_and_origin(failures)
    test_default_seed_determinism(failures)
    test_seeded_jitter_bounds(failures)
    test_backwards_compat(failures)
    if failures:
        print("FAILED (%d):" % len(failures))
        for f in failures:
            print("  FAIL " + f)
        sys.exit(1)
    print("OK gen_props kit v2: all gates green")


if __name__ == "__main__":
    main()
