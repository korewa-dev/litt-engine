#!/usr/bin/env python3
"""Litt Play launcher - one command (or double-click PLAY.bat) to play the world.

Checks the live server is up and actually serving the PLAYER page (not just
any 200); starts it if needed; opens your default browser. No CDN dependency:
three.js is vendored next to play.html.
"""
import subprocess
import sys
import time
import urllib.request
import webbrowser
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLAY_URL = "http://127.0.0.1:8088/viewer/play.html"

def server_ok():
    try:
        with urllib.request.urlopen(PLAY_URL, timeout=3) as r:
            body = r.read().decode("utf-8", "replace")
            return r.status == 200 and "Litt Play" in body
    except Exception:
        return False

def main():
    if server_ok():
        print("[play] server already running with player page")
    else:
        print("[play] starting live server...")
        subprocess.Popen(
            [sys.executable, str(HERE / "tools" / "serve_live.py")],
            cwd=str(HERE),
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        for _ in range(40):
            time.sleep(0.25)
            if server_ok():
                break
        else:
            raise SystemExit("[play] server did not come up - run tools/serve_live.py manually and read its error")
    webbrowser.open(PLAY_URL)
    print("[play] opened " + PLAY_URL + " in your browser")
    print("[play] controls: WASD move | Space jump | click = mouse look | Esc releases mouse")

if __name__ == "__main__":
    main()