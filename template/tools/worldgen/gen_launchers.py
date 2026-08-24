#!/usr/bin/env python3
"""gen_launchers.py - write ENGINE.bat (Windows) AND ENGINE.sh (Linux/macOS)
into every Project/<game> so a built world launches natively on any OS:

    Windows : double-click ENGINE.bat          -> litt play <dir>
    Linux   : ./ENGINE.sh                      -> "$LITT_ENGINE" play <dir>

Resolution order: $LITT_ENGINE env var -> release exe -> debug exe.
Replaces the old gen_engine_bats.py (bat-only).
"""
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]

BAT = """@echo off
rem Litt native launcher - engine plays this world in its own window
setlocal
set "HERE=%~dp0"
set "ROOT=%HERE%..\\..\\"
if defined LITT_ENGINE set "EXE=%LITT_ENGINE%"
if not defined EXE set "EXE=%ROOT%target\\x86_64-pc-windows-gnu\\release\\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\\x86_64-pc-windows-gnu\\debug\\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\\release\\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\\debug\\litt.exe"
"%EXE%" play "%HERE:~0,-1%"
"""

SH = """#!/bin/sh
# Litt native launcher - engine plays this world in its own window
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
EXE="$LITT_ENGINE"
[ -n "$EXE" ] || EXE="$ROOT/target/x86_64-pc-windows-gnu/release/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/x86_64-pc-windows-gnu/debug/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/release/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/debug/litt"
exec "$EXE" play "$HERE"
"""


def main() -> None:
    made = 0
    for game in sorted((REPO / "Project").iterdir()):
        scene = game / "assets" / "scenes" / "world.lscn.json"
        if not scene.exists():
            continue
        bat = game / "ENGINE.bat"
        sh = game / "ENGINE.sh"
        bat.write_text(BAT.replace("\\\\", "\\"), encoding="ascii", newline="\r\n")
        sh.write_text(SH, encoding="utf-8", newline="\n")
        sh.chmod(0o755)
        made += 1
        print("[launchers] %s: ENGINE.bat + ENGINE.sh" % game.name)
    print("total: %d" % made)


if __name__ == "__main__":
    main()
