#!/usr/bin/env python3
"""Compare WorldGen reports emitted on two operating systems."""
import json
import sys
from pathlib import Path

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def main():
    if len(sys.argv) != 3:
        print("usage: compare_worldgen_reports.py REPORT_A REPORT_B", file=sys.stderr)
        return 2
    a, b = load(sys.argv[1]), load(sys.argv[2])
    failures, drift = [], []
    names = sorted(set(a.get("cases", {})) | set(b.get("cases", {})))
    for name in names:
        ca, cb = a.get("cases", {}).get(name), b.get("cases", {}).get(name)
        if ca is None or cb is None:
            failures.append("%s missing from one report" % name)
            continue
        for label, case in (("A", ca), ("B", cb)):
            if not case.get("deterministic"):
                failures.append("%s is nondeterministic on platform %s" % (name, label))
            if case.get("reference_problems"):
                failures.append("%s has reference problems on platform %s" % (name, label))
        da, db = ca.get("structure_digest"), cb.get("structure_digest")
        if not da or not db:
            failures.append("%s missing structure digest" % name)
        elif da != db:
            # Keep this visible without rejecting otherwise valid projects:
            # platform libm differences can alter procedural placement around
            # geometric thresholds. Exact same-host determinism remains gated.
            drift.append("%s: %s != %s" % (name, da, db))
    if drift:
        print("Cross-OS structural drift detected (tracked, non-blocking):")
        for item in drift:
            print(" - " + item)
    if failures:
        print("Cross-OS WorldGen contract FAILED:")
        for failure in failures:
            print(" - " + failure)
        return 1
    print("Cross-OS WorldGen contract passed for %d cases; structural drift=%d." %
          (len(names), len(drift)))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
