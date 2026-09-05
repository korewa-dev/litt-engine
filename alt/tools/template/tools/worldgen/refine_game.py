#!/usr/bin/env python3
"""refine_game.py - CDR-010 agentic refine loop (WorldClaw principle 5).

Closes the generate -> critique -> REFINE cycle around the shipped pipeline:

    for attempt k in 0..N-1:
        seed_k = base_seed + k * 1009          (documented sequence)
        make_game.py --kind K --seed seed_k -> scratch dir (%TEMP%)
        native_proof.proof_one_game(scratch) -> authoritative record
        composite score = (sim_ok, missing==0, fill, colors, rows_span)
    first FULLY-passing attempt -> deploy to --out-dir/<name>, ATTEMPTS
    trail appended to its NOTES.md, scratch cleaned, exit 0.
    otherwise the BEST-scoring candidate ships to <name>-rejected UNDER
    %TEMP% (never Project/) with failed assertions + suspect assets,
    machine-readable JSON on the last stdout line, exit 1.

Determinism: identical --base-seed => identical attempt sequence. No clock,
no undeduced randomness anywhere in this script.

Run:
  python refine_game.py --kind space --name my-game --base-seed 42 \
      --attempts 3 --out-dir Project --min-fill 1.5 --min-colors 8 \
      [--yaw-delta] [--force]

Last stdout line is machine-readable JSON including the full attempt trail.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent            # template/tools/worldgen
REPO = HERE.parent.parent.parent                  # engine root
ASSETS_TOOLS = REPO / "template" / "tools" / "assets"
sys.path.insert(0, str(ASSETS_TOOLS))
from native_proof import proof_one_game, yaw_delta_check  # noqa: E402

MAKE_GAME = HERE / "make_game.py"
KINDS = ("soulslike", "space", "tabletop", "platformer25d", "archetype")
SEED_STRIDE = 1009          # CDR-010 documented sequence: seed_k = base + k*1009
MAKE_TIMEOUT_S = 900        # generous wall-clock cap per generation attempt


# --------------------------------------------------------------- utilities
def bin_paths():
    cli = REPO / "native" / "bin" / ("littcli.exe" if os.name == "nt"
                                     else "littcli")
    view = REPO / "native" / "bin" / ("littview.exe" if os.name == "nt"
                                      else "littview")
    return cli, view


def json_tail(text):
    """Last {...} line of a tool's stdout (make_game/littcli contract)."""
    for ln in reversed((text or "").strip().splitlines()):
        ln = ln.strip()
        if ln.startswith("{"):
            try:
                return json.loads(ln)
            except Exception:
                continue
    return {}


def rows_span(rec):
    """Numeric R part of proof_one_game's 'R/H' rows string."""
    try:
        return int(str(rec.get("rows", "0/0")).split("/")[0])
    except Exception:
        return 0


def score_of(rec):
    """Composite lexicographic score tuple (CDR-010 motion item 2)."""
    return (rec.get("sim") == "ok",
            rec.get("missing") == 0,
            float(rec.get("fill_pct", 0.0)),
            int(rec.get("colors", 0)),
            rows_span(rec))


def is_full_pass(rec, min_fill, min_colors, want_yaw):
    if rec.get("sim") != "ok" or rec.get("missing") != 0:
        return False
    if float(rec.get("fill_pct", 0.0)) < min_fill:
        return False
    if int(rec.get("colors", 0)) < min_colors:
        return False
    if want_yaw and (rec.get("yaw_delta") or {}).get("verdict") != "PASS":
        return False
    return True


def failed_assertions(rec, min_fill, min_colors):
    """Every failed gate of ONE attempt, named, with got/want/margin."""
    out = []
    if rec.get("sim") != "ok":
        out.append({"assert": "sim", "got": rec.get("sim"), "want": "ok",
                    "margin": None})
    m = rec.get("missing")
    if m != 0:
        out.append({"assert": "missing", "got": m, "want": 0,
                    "margin": m})
    f = round(float(rec.get("fill_pct", 0.0)), 2)
    if f < min_fill:
        out.append({"assert": "fill", "got": f, "want": min_fill,
                    "margin": round(f - min_fill, 2)})
    c = int(rec.get("colors", 0))
    if c < min_colors:
        out.append({"assert": "colors", "got": c, "want": min_colors,
                    "margin": c - min_colors})
    if (rec.get("yaw_delta") or {}).get("verdict") == "FAIL":
        yd = rec["yaw_delta"]
        out.append({"assert": "yaw_delta",
                    "got": {"silhouette_diff_pct": yd.get("silhouette_diff_pct"),
                            "fill_keep_pct": yd.get("fill_keep_pct")},
                    "want": "PASS", "margin": None,
                    "problems": yd.get("problems", [])})
    return out


def suspect_assets(game_dir, top=3):
    """Top-N largest models by triangle count, from asset_index.json.

    Diagnosis heuristic (CDR-010 motion item 4): underfilled frames are most
    likely caused by the heaviest meshes swamping/drowning the frame."""
    gdir = Path(game_dir)
    models = []
    idx = gdir / "assets" / "asset_index.json"
    paths = []
    try:
        for a in json.loads(idx.read_text(encoding="utf-8")).get("assets", []):
            if a.get("type") == "model" and a.get("path"):
                paths.append(a["path"])
    except Exception:
        pass
    if not paths:
        mdir = gdir / "assets" / "models"
        paths = sorted(p.name for p in mdir.glob("*.obj")) if mdir.is_dir() else []
    for p in paths:
        obj = gdir / "assets" / p
        if not obj.exists():
            obj = gdir / "assets" / "models" / Path(p).name
        if not obj.exists():
            continue
        tris = 0
        try:
            with open(obj, "r", encoding="utf-8", errors="replace") as fh:
                for ln in fh:
                    if ln.startswith("f "):
                        tris += 1
        except OSError:
            continue
        models.append((tris, Path(p).name))
    models.sort(reverse=True)
    return ["%s (tris %d)" % (n, t) for t, n in models[:top]]


def manifest_entry_fix(name, snapshot, deployed=None, winner_seed=None):
    """Keep Project/games.json truthful about OUR runs (best effort).

    make_game always registers the built dir - including scratch dirs under
    %TEMP%. On success we repoint the entry at the deployed game; on overall
    failure we restore the pre-run entry verbatim. Never touches other
    entries; failures here are non-fatal."""
    p = REPO / "Project" / "games.json"
    try:
        man = json.loads(p.read_text(encoding="utf-8"))
        games = [g for g in man.get("games", [])
                 if isinstance(g, dict) and g.get("name")]
        before = json.dumps(games, sort_keys=True)
        kept = [g for g in games if g["name"] != name]
        entry = dict(snapshot) if snapshot else {
            "name": name, "about": "", "built_by": "refine_game.py"}
        if deployed is not None:
            entry.update({"name": name, "dir": deployed,
                          "seed": winner_seed})
            try:
                entry["dir"] = str(Path(deployed).relative_to(REPO))
            except ValueError:
                entry["dir"] = str(deployed)
            kept.append(entry)
        elif snapshot:
            kept.append(dict(snapshot))
        man["games"] = kept
        if json.dumps(kept, sort_keys=True) != before:
            p.write_text(json.dumps(man, indent=2), encoding="utf-8")
    except Exception as exc:                        # hygiene is best-effort
        print("[refine] games.json upkeep skipped: %s" % exc, file=sys.stderr)


def snapshot_entry(name):
    try:
        man = json.loads((REPO / "Project" / "games.json")
                         .read_text(encoding="utf-8"))
        for g in man.get("games", []):
            if isinstance(g, dict) and g.get("name") == name:
                return dict(g)
    except Exception:
        pass
    return None


def append_attempts_section(notes_path, trail, args, winner_k):
    lines = ["", "## ATTEMPTS (refine_game.py - CDR-010 refine loop)", "",
             "- thresholds: fill >= %.1f%%, colors >= %d%s"
             % (args.min_fill, args.min_colors,
                ", yaw-delta on" if args.yaw_delta else ""),
             "- seed sequence: seed_k = %d + k*%d" % (args.base_seed,
                                                      SEED_STRIDE),
             "- winner: attempt %d (seed %d)"
             % (winner_k, trail[winner_k]["seed"]),
             "",
             "| k | seed | fill %% | colors | rows | sim | missing | verdict |"
             " score |",
             "|---|------|--------|--------|------|-----|---------|---------|"
             "-------|"]
    for t in trail:
        lines.append(
            "| %d | %d | %.2f | %s | %s | %s | %s | %s | %s |"
            % (t["k"], t["seed"], t["fill"], t["colors"], t["rows"],
               t["sim"], t["missing"], t["verdict"], tuple(t["score"])))
        for pr in (t.get("problems") or [])[:4]:
            lines.append("| | | | | | | | ^ %s | |" % pr.replace("|", "/"))
    with open(notes_path, "a", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


# ----------------------------------------------------------------- attempt
def run_attempt(k, seed, name, kind, scratch_root, cli, view,
                min_fill, min_colors, want_yaw):
    """Build ONE candidate in a scratch dir and score it authoritatively."""
    scratch = scratch_root / ("refine-%s-k%d" % (name, k))
    if scratch.exists():                       # leftover from an old run
        shutil.rmtree(scratch)
    print("[refine] attempt %d: building seed=%d -> %s" % (k, seed, scratch))
    cmd = [sys.executable, str(MAKE_GAME), "--kind", kind, "--name", name,
           "--seed", str(seed), "--out-dir", str(scratch)]
    make_ok = False
    make_rc = None
    make_json = {}
    try:
        child_env = dict(os.environ, LITT_NO_MANIFEST="1")  # scratch: no reg
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=MAKE_TIMEOUT_S, env=child_env)
        make_rc = r.returncode
        make_json = json_tail(r.stdout)
        make_ok = (r.returncode == 0 and make_json.get("ok") is True)
        if not make_ok:
            sys.stderr.write((r.stderr or "")[-800:] + "\n")
    except subprocess.TimeoutExpired:
        sys.stderr.write("[refine] attempt %d: make_game TIMEOUT\n" % k)

    rec = {"game": "%s-k%d" % (name, k)}
    if (scratch / "assets" / "scenes" / "world.lscn.json").exists():
        rec = proof_one_game(scratch, cli, view, min_fill, min_colors,
                             frames=60)
        rec["game"] = "%s-k%d" % (name, k)
    else:
        rec.update({"sim": "FAIL", "missing": None, "fill": 0.0,
                    "fill_pct": 0.0, "colors": 0, "rows": "0/0",
                    "h": 0,
                    "problems": ["make_game produced no world scene"]})
    rec["problems"] = list(rec.get("problems") or [])
    if not make_ok:
        rec["problems"].insert(0, "make_game ok:true (got rc=%s json_ok=%s)"
                                  % (make_rc, make_json.get("ok")))
    if want_yaw and view and Path(view).exists():
        rec["yaw_delta"] = yaw_delta_check(scratch, view)
    rec["score"] = list(score_of(rec))
    rec["full_pass"] = is_full_pass(rec, min_fill, min_colors, want_yaw)
    rec.update({"k": k, "seed": seed, "dir": str(scratch)})
    print("[refine] attempt %d seed=%d fill=%.1f%% colors=%d -> score=%s"
          % (k, seed, rec.get("fill_pct", 0.0), rec.get("colors", 0),
             tuple(rec["score"])))
    return rec, scratch


def slim(trail):
    """Compact JSON-safe attempt trail for the final machine line."""
    out = []
    for t in trail:
        e = {kk: t.get(kk) for kk in
             ("k", "seed", "sim", "mode", "interactives", "missing",
              "fill", "colors", "rows", "verdict", "score", "full_pass",
              "problems")}
        if "yaw_delta" in t:
            e["yaw_delta"] = {kk: t["yaw_delta"].get(kk) for kk in
                              ("verdict", "silhouette_diff_pct",
                               "fill_keep_pct", "problems")}
        out.append(e)
    return out


# -------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(
        description="CDR-010 refine loop: generate -> prove -> diagnose -> "
                    "re-seed -> deploy (wraps make_game + native_proof)")
    ap.add_argument("--kind", default="archetype", choices=list(KINDS))
    ap.add_argument("--name", default=None,
                    help="game name; default auto slug incl. base seed")
    ap.add_argument("--base-seed", type=int, default=0,
                    help="attempt k uses seed base_seed + k*%d" % SEED_STRIDE)
    ap.add_argument("--attempts", type=int, default=3)
    ap.add_argument("--out-dir", default=None,
                    help="deploy root (default Project/) - generation always"
                         " happens in %%TEMP%% scratch dirs")
    ap.add_argument("--min-fill", type=float, default=1.5)
    ap.add_argument("--min-colors", type=int, default=8)
    ap.add_argument("--yaw-delta", action="store_true",
                    help="also require the two-yaw displacement probe to PASS")
    ap.add_argument("--force", action="store_true",
                    help="allow overwriting an existing deploy dir")
    args = ap.parse_args()

    if args.attempts < 1:
        ap.error("--attempts must be >= 1")
    cli, view = bin_paths()
    missing_bins = [b for b in (cli, view) if not Path(b).exists()]
    if missing_bins:
        print(json.dumps({"ok": False, "error": "native binaries missing",
                          "missing": [str(b) for b in missing_bins]}))
        return 2

    base = re.sub(r"[^a-z0-9]+", "-", "%s-%d" % (args.kind, args.base_seed)
                  ).strip("-")
    name = args.name or base                       # auto slug incl. seed
    out_root = Path(args.out_dir) if args.out_dir else REPO / "Project"
    deploy = out_root / name
    if deploy.exists() and not args.force:
        print(json.dumps({"ok": False, "error": "deploy dir exists",
                          "deployed": str(deploy),
                          "hint": "pass --force to overwrite"}))
        return 2

    snap = snapshot_entry(name)
    scratch_root = Path(tempfile.gettempdir())
    trail, scratches = [], []
    winner_k = None
    for k in range(args.attempts):
            seed = args.base_seed + k * SEED_STRIDE      # documented sequence
            rec, scratch = run_attempt(k, seed, name, args.kind,
                                       scratch_root, cli, view,
                                       args.min_fill, args.min_colors,
                                       args.yaw_delta)
            trail.append(rec)
            scratches.append(scratch)
            if rec["full_pass"]:
                winner_k = k
                break

    if winner_k is not None:
        win = trail[winner_k]
        if deploy.exists():                    # --force re-deploy
            shutil.rmtree(deploy)
        shutil.copytree(win["dir"], deploy)
        notes = deploy / "NOTES.md"
        if notes.exists():
            append_attempts_section(notes, trail, args, winner_k)
        manifest_entry_fix(name, snap, deployed=str(deploy),
                           winner_seed=win["seed"])
        for s in scratches:
            shutil.rmtree(s, ignore_errors=True)
        print("[refine] WINNER attempt %d seed=%d deployed -> %s"
              % (winner_k, win["seed"], deploy))
        print(json.dumps({"ok": True, "deployed": str(deploy),
                          "attempts": slim(trail), "winner": winner_k}))
        return 0

    # ---- all attempts failed: ship BEST candidate to %TEMP%, diagnose -----
    best_k = max(range(len(trail)), key=lambda i: tuple(trail[i]["score"]))
    best = trail[best_k]
    tmp = Path(tempfile.gettempdir()).resolve()
    try:
        under_tmp = (out_root.resolve()).is_relative_to(tmp)
    except AttributeError:                     # pre-3.9 fallback
        under_tmp = str(out_root.resolve()).lower().startswith(
            str(tmp).lower())
    rejected = ((out_root / (name + "-rejected")) if under_tmp
                else tmp / (name + "-rejected"))
    if rejected.exists():
        shutil.rmtree(rejected)
    shutil.copytree(best["dir"], rejected)
    report = {"ok": False, "verdict": "REJECTED",
              "thresholds": {"min_fill": args.min_fill,
                             "min_colors": args.min_colors,
                             "yaw_delta": bool(args.yaw_delta)},
              "attempts": slim(trail), "best_attempt": best_k}
    (rejected / "REFINE_REPORT.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    rnotes = rejected / "NOTES.md"
    if rnotes.exists():
        with open(rnotes, "a", encoding="utf-8") as fh:
            fh.write("\n\n## REFINE VERDICT\n\n- REJECTED after %d attempt(s);"
                     " best was attempt %d (seed %d)\n- shipped here for"
                     " diagnosis; see REFINE_REPORT.json\n"
                     % (len(trail), best_k, best["seed"]))
    assertions = failed_assertions(best, args.min_fill, args.min_colors)
    suspects = suspect_assets(best["dir"])
    manifest_entry_fix(name, snap, deployed=None)
    for s in scratches:
        shutil.rmtree(s, ignore_errors=True)
    print("[refine] ALL %d attempt(s) failed; best attempt %d (seed %d, "
          "score=%s) shipped -> %s"
          % (len(trail), best_k, best["seed"], tuple(best["score"]),
             rejected))
    print(json.dumps({"ok": False,
                      "best": {"k": best_k, "seed": best["seed"],
                               "score": best["score"], "fill": best["fill"],
                               "colors": best["colors"], "rows": best["rows"],
                               "sim": best["sim"], "missing": best["missing"],
                               "rejected_dir": str(rejected)},
                      "attempts": slim(trail),
                      "failed_assertions": assertions,
                      "suspects": suspects}))
    return 1


if __name__ == "__main__":
    sys.exit(main())
