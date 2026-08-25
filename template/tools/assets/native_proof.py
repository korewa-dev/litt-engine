#!/usr/bin/env python3
"""native_proof.py - REAL proof that every game project renders natively.

For each game project:
  1. `littcli validate <dir>` - headless C simulation (solids, entities,
     contract physics, zero missing models)
  2. `littview render <dir> --out tmp.bmp` - the C++ front-end bakes the
     scene and rasterizes a frame; we then assert real pixel content
     (frame fill, color diversity, vertical span) so "renders" means
     "a human would see a world", not just "exit 0".

Run:  python template/tools/assets/native_proof.py [--min-fill 3.0]
      [... --yaw-delta]   (audit 5.4: opt-in two-yaw displacement probe)
Exit 0 only if every game validates AND shows real rendered content.

Importable pieces:
  bmp_stats(path)          - fill % / color families / vertical span of a BMP
  proof_one_game(...)      - full simulate+render+gate proof for ONE game dir
                             (worldgen/make_game.py calls this instead of
                             duplicating the logic)
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).parent
REPO = HERE.parent.parent.parent
PROJECTS = REPO / "Project"
BIN = REPO / "native" / "bin"


def _bmp_pixels(path):
    """(w, h, offset, row_stride, raw bytes) of a 24-bit BMP."""
    d = open(path, "rb").read()
    w, h = struct.unpack("<ii", d[18:26])
    off = struct.unpack("<I", d[10:14])[0]
    row = (w * 3 + 3) & ~3
    return w, h, off, row, d


def bmp_stats(path, clear=(15, 24, 32), stride=2):
    w, h, off, row, d = _bmp_pixels(path)
    cnt = 0
    colors = set()
    rows = set()
    total = 0
    for y in range(0, h, stride):
        base = off + (h - 1 - y) * row
        for x in range(0, w, stride):
            t = tuple(d[base + x * 3:base + x * 3 + 3])
            total += 1
            if t != clear:
                cnt += 1
                colors.add((t[0] // 24, t[1] // 24, t[2] // 24))
                rows.add(y)
    return {"fill": 100.0 * cnt / max(1, total),
            "colors": len(colors), "rows": len(rows) * stride, "h": h}


def bmp_silhouette_diff(path_a, path_b, clear=(15, 24, 32), stride=4):
    """Fraction of sampled pixels whose background/foreground mask differs
    between two same-size renders. ~0 means the two views are silhouette-
    identical (no parallax -> flat billboard or camera bug)."""
    wa, ha, offa, rowa, da = _bmp_pixels(path_a)
    wb, hb, offb, rowb, db = _bmp_pixels(path_b)
    if (wa, ha) != (wb, hb):
        return 1.0                      # framing changed entirely
    total = diff = 0
    for y in range(0, ha, stride):
        ba = offa + (ha - 1 - y) * rowa
        bb = offb + (hb - 1 - y) * rowb
        for x in range(0, wa, stride):
            ta = tuple(da[ba + x * 3:ba + x * 3 + 3]) != clear
            tb = tuple(db[bb + x * 3:bb + x * 3 + 3]) != clear
            total += 1
            if ta != tb:
                diff += 1
    return diff / max(1, total)


def proof_one_game(game_dir, cli, view, min_fill=1.5, min_colors=8,
                   frames=60, sim=None):
    """Native proof for ONE game directory (module-level, importable).

    1. `littcli validate --frames N` unless the caller already validated and
       passes its parsed JSON via `sim`;
    2. `littview render` into a temp BMP measured with bmp_stats;
    3. pixel gates: fill >= min_fill %% AND color families >= min_colors.

    Returns a plain record dict (JSON-safe):
      game, sim ('ok'|'FAIL'), mode, interactives, missing,
      fill (rounded 1dp), fill_pct (raw), colors, rows ('R/H'), h,
      verdict ('PASS'|'FAIL'), problems ([str] every failed assertion).
    """
    gdir = Path(game_dir)
    rec = {"game": gdir.name}
    problems = []

    # 1) headless simulation (skipped when the caller supplies `sim`)
    js = sim if isinstance(sim, dict) else None
    if js is None and cli and Path(cli).exists():
        r = subprocess.run([str(cli), "validate", str(gdir),
                            "--frames", str(frames)],
                           capture_output=True, text=True, timeout=120)
        try:
            js = json.loads(r.stdout.strip().splitlines()[-1])
        except Exception:
            js = None
    if isinstance(js, dict):
        rec["sim"] = "ok" if js.get("ok") else "FAIL"
        rec["mode"] = js.get("mode")
        rec["interactives"] = js.get("interactives")
        rec["missing"] = js.get("missing")
    else:
        rec["sim"] = "FAIL"
    if rec["sim"] != "ok":
        problems.append("validate ok:true")

    # 2) real pixels: one rendered frame must show actual content
    st = {}
    bmp = Path(tempfile.gettempdir()) / ("littproof_%s.bmp" % rec["game"])
    try:
        if view and Path(view).exists():
            rv = subprocess.run([str(view), "render", str(gdir),
                                 "--out", str(bmp)],
                                capture_output=True, text=True, timeout=120)
            if rv.returncode == 0 and bmp.exists():
                st = bmp_stats(str(bmp))
            else:
                problems.append("littview render exit 0 + BMP written "
                                "(rc=%s bmp=%s)"
                                % (rv.returncode, bmp.exists()))
        else:
            problems.append("littview binary present at %s" % view)
    finally:
        bmp.unlink(missing_ok=True)

    rec["fill"] = round(st.get("fill", 0.0), 1)
    rec["fill_pct"] = st.get("fill", 0.0)
    rec["colors"] = st.get("colors", 0)
    rec["rows"] = "%d/%d" % (st.get("rows", 0), st.get("h", 0))
    rec["h"] = st.get("h", 0)

    ok_pix = (st.get("fill", 0.0) >= min_fill
              and st.get("colors", 0) >= min_colors)
    if st.get("fill", 0.0) < min_fill:
        problems.append("rendered fill >= %.1f%% (got %.2f%%)"
                        % (min_fill, st.get("fill", 0.0)))
    if st.get("colors", 0) < min_colors:
        problems.append("color families >= %d (got %d)"
                        % (min_colors, st.get("colors", 0)))

    rec["verdict"] = "PASS" if (rec["sim"] == "ok" and ok_pix) else "FAIL"
    rec["problems"] = problems
    return rec


# Audit 5.4: fixed yaw pair ~52 deg apart, both near littview's auto framing.
YAW_PAIR = (0.7, 1.6)


def yaw_delta_check(game_dir, view, min_diff=0.005, min_fill_keep=0.35):
    """Displacement-sensitive pixel assertion (audit 5.4, opt-in).

    Renders the SAME game at two fixed camera yaws and requires:
      * silhouettes DIFFER between the yaws (real 3D parallax - a flat
        billboard, a degenerate scene, or a stuck camera fails here), and
      * the world stays in frame from the second yaw (fill retention -
        double-transform class bugs fling content off-centre/explode it,
        which collapses the second view toward empty).
    Returns {"yaws", "silhouette_diff_pct", "fill_keep_pct",
             "verdict", "problems"}."""
    gdir = Path(game_dir)
    stats = []
    bmps = []
    try:
        for yaw in YAW_PAIR:
            p = Path(tempfile.gettempdir()) / (
                "littyaw_%s_%.2f.bmp" % (gdir.name, yaw))
            bmps.append(p)
            rv = subprocess.run(
                [str(view), "render", str(gdir), "--yaw", str(yaw),
                 "--out", str(p)],
                capture_output=True, text=True, timeout=120)
            if rv.returncode != 0 or not p.exists():
                return {"verdict": "FAIL",
                        "problems": ["yaw render exit 0 at yaw=%.2f" % yaw]}
            stats.append(bmp_stats(str(p)))
        diff = bmp_silhouette_diff(str(bmps[0]), str(bmps[1]))
        fill_keep = stats[1]["fill"] / max(stats[0]["fill"], 1e-6)
        problems = []
        if diff < min_diff:
            problems.append("silhouette yaw-diff >= %.1f%% (got %.2f%%)"
                            % (min_diff * 100, diff * 100))
        if fill_keep < min_fill_keep:
            problems.append("second-yaw fill kept >= %.0f%% (got %.0f%%)"
                            % (min_fill_keep * 100, fill_keep * 100))
        return {"yaws": list(YAW_PAIR),
                "silhouette_diff_pct": round(diff * 100, 2),
                "fill_keep_pct": round(fill_keep * 100, 1),
                "verdict": "PASS" if not problems else "FAIL",
                "problems": problems}
    finally:
        for p in bmps:
            p.unlink(missing_ok=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-fill", type=float, default=1.5,
                    help="minimum %% of frame that must be non-clear")
    ap.add_argument("--min-colors", type=int, default=8,
                    help="minimum distinct color families (/24 buckets)")
    ap.add_argument("--yaw-delta", action="store_true",
                    help="opt-in audit-5.4 probe: render each game at two "
                         "camera yaws and assert silhouette parallax plus "
                         "second-yaw framing (displacement-sensitive)")
    args = ap.parse_args()

    cli = BIN / ("littcli.exe" if os.name == "nt" else "littcli")
    view = BIN / ("littview.exe" if os.name == "nt" else "littview")
    if not cli.exists() or not view.exists():
        print("native binaries missing - build with native/build.bat",
              file=sys.stderr)
        return 2

    games = sorted(p for p in PROJECTS.iterdir()
                   if p.is_dir() and (p / "world_state.json").exists()
                   and (p / "story").is_dir())  # story/ = shipped game;
                                                 # sandbox dirs are skipped
    if not games:
        print("no shippable games found", file=sys.stderr)
        return 2
    bad = 0
    results = []
    for g in games:
        rec = proof_one_game(g, cli, view, args.min_fill, args.min_colors)
        if args.yaw_delta:
            rec["yaw_delta"] = yaw_delta_check(g, view)
            if rec["yaw_delta"]["verdict"] == "FAIL":
                rec["verdict"] = "FAIL"
        if rec["verdict"] == "FAIL":
            bad += 1
        results.append(rec)

    for rec in results:
        print("%-18s sim=%-4s mode=%-8s inter=%-4s "
              "fill=%5.1f%% cols=%-3d rows=%-8s %-4s%s"
              % (rec["game"], rec["sim"], rec.get("mode", "?"),
                 rec.get("interactives", "?"), rec["fill"], rec["colors"],
                 rec["rows"], rec["verdict"],
                 (" yaw=%s" % rec["yaw_delta"]["verdict"])
                 if "yaw_delta" in rec else ""))
        if rec["verdict"] == "FAIL":
            for p in rec.get("problems", []):
                print("                  expected %s" % p, file=sys.stderr)
            if "yaw_delta" in rec:
                for p in rec["yaw_delta"]["problems"]:
                    print("                  yaw-delta: expected %s" % p,
                          file=sys.stderr)
    print("\n%d/%d games pass native proof (min_fill=%.1f%% min_colors=%d%s)"
          % (len(results) - bad, len(results), args.min_fill,
             args.min_colors, " yaw-delta=on" if args.yaw_delta else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
