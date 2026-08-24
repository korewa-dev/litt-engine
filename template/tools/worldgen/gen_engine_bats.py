#!/usr/bin/env python3
"""One-shot: write ENGINE.bat into every Project/<game> so each folder
launches itself inside the Litt Studio window (chat + live viewport)."""
from pathlib import Path

# file -> worldgen -> tools -> template -> engine root
REPO = Path(__file__).resolve().parents[3]

TEMPLATE = """@echo off
rem {name} in the Litt Studio window
setlocal
set "ROOT=%~dp0..\\.."
if defined LITT_ENGINE set "EXE=%LITT_ENGINE%"
if not defined LITT_ENGINE (
  if exist "%ROOT%\\target\\x86_64-pc-windows-gnu\\release\\litt.exe" set "EXE=%ROOT%\\target\\x86_64-pc-windows-gnu\\release\\litt.exe"
)
if not defined EXE if exist "%ROOT%\\target\\x86_64-pc-windows-gnu\\debug\\litt.exe" set "EXE=%ROOT%\\target\\x86_64-pc-windows-gnu\\debug\\litt.exe"
if not defined EXE (
  echo Litt engine exe not found - build it first ^(cargo build^) or set LITT_ENGINE
  pause
  exit /b 1
)
"%EXE%" studio "%~dp0"
"""

count = 0
for gdir in sorted((REPO / "Project").iterdir()):
    scene = gdir / "assets" / "scenes" / "world.lscn.json"
    if not gdir.is_dir() or not scene.exists():
        continue
    bat = TEMPLATE.format(name=gdir.name)
    (gdir / "ENGINE.bat").write_text(bat, encoding="ascii", newline="\r\n")
    print("ENGINE.bat ->", gdir.name)
    count += 1
print("total:", count)
