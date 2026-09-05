#!/usr/bin/env python3
"""Merges design_extra.json archetypes into design_rules.json. Safe to re-run."""
import json
from pathlib import Path

here = Path(__file__).parent
rules = here / "design_rules.json"
extra = here / "design_extra.json"
data = json.loads(rules.read_text(encoding="utf-8"))
pack = json.loads(extra.read_text(encoding="utf-8"))
added = skipped = 0
for k, v in pack["archetypes"].items():
    if k in data["archetypes"]:
        skipped += 1
        continue
    data["archetypes"][k] = v
    added += 1
rules.write_text(json.dumps(data, indent=2) + chr(10), encoding="utf-8")
print("merged: +%d new, %d already present, total %d archetypes" % (added, skipped, len(data["archetypes"])))