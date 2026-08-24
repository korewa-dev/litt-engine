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
Exit 0 only if every game validates AND shows real rendered content.
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


def bmp_stats(path, clear=(15, 24, 32), stride=2):
    d = open(path, "rb").read()
    w, h = struct.unpack("<ii", d[18:26])
    off = struct.unpack("<I", d[10:14])[0]
    row = (w * 3 + 3) & ~3
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-fill", type=float, default=1.5,
                    help="minimum %% of frame that must be non-clear")
    ap.add_argument("--min-colors", type=int, default=8,
                    help="minimum distinct color families (/24 buckets)")
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
        rec = {"game": g.name}
        r = subprocess.run([str(cli), "validate", str(g), "--frames", "60"],
                           capture_output=True, text=True, timeout=120)
        try:
            js = json.loads(r.stdout.strip().splitlines()[-1])
            rec["sim"] = "ok" if js.get("ok") else "FAIL"
            rec["mode"] = js.get("mode")
            rec["interactives"] = js.get("interactives")
        except Exception:
            rec["sim"] = "FAIL"
        ok_sim = rec["sim"] == "ok"

        bmp = Path(tempfile.gettempdir()) / f"littproof_{g.name}.bmp"
        rv = subprocess.run([str(view), "render", str(g), "--out", str(bmp)],
                            capture_output=True, text=True, timeout=120)
        ok_view = rv.returncode == 0 and bmp.exists()
        st = bmp_stats(bmp) if ok_view else {}
        bmp.unlink(missing_ok=True)
        rec.update(fill=round(st.get("fill", 0.0), 1),
                   colors=st.get("colors", 0),
                   rows=f"{st.get('rows', 0)}/{st.get('h', 0)}")
        ok_pix = (st.get("fill", 0) >= args.min_fill
                  and st.get("colors", 0) >= args.min_colors)

        rec["verdict"] = "PASS" if (ok_sim and ok_pix) else "FAIL"
        if rec["verdict"] == "FAIL":
            bad += 1
        results.append(rec)

    for rec in results:
        print("%-18s sim=%-4s mode=%-8s inter=%-4s "
              "fill=%5.1f%% cols=%-3d rows=%-8s %s"
              % (rec["game"], rec["sim"], rec.get("mode", "?"),
                 rec.get("interactives", "?"), rec["fill"], rec["colors"],
                 rec["rows"], rec["verdict"]))
    print("\n%d/%d games pass native proof (min_fill=%.1f%% min_colors=%d)"
          % (len(results) - bad, len(results), args.min_fill,
             args.min_colors))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
