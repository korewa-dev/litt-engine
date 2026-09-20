#!/usr/bin/env python3
"""Compare canonical WorldGen digests emitted on two operating systems."""
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
    failures = []
    names = sorted(set(a.get("cases", {})) | set(b.get("cases", {})))
    for name in names:
        ca, cb = a.get("cases", {}).get(name), b.get("cases", {}).get(name)
        if ca is None or cb is None:
            failures.append("%s missing from one report" % name)
            continue
        da = ca.get("structure_digest")
        db = cb.get("structure_digest")
        if not da or not db:
            failures.append("%s missing structure digest" % name)
        elif da != db:
            failures.append("%s generated structure differs: %s != %s" %
                            (name, da, db))
    if failures:
        print("Cross-OS WorldGen determinism FAILED:")
        for failure in failures:
            print(" - " + failure)
        return 1
    print("Cross-OS WorldGen determinism passed for %d cases." % len(names))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
