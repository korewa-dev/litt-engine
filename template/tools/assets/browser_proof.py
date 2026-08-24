#!/usr/bin/env python3
"""browser_proof.py - REAL headless-Chromium proof that every game project
plays in the browser runtime. Serves each game, opens play.html, collects
console errors + page errors, waits for frames to advance (player spawns,
world draws), and screenshots what a human would see.

Run:  python template/tools/assets/browser_proof.py [--seconds 4]
Exit 0 only if every game renders with zero console/page errors.
"""
import argparse
import json
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

HERE = Path(__file__).parent
REPO = HERE.parent.parent.parent
PROJECTS = REPO / "Project"
SHOTS = HERE / "browser_shots"


def free_port(start):
    import socket
    port = start
    while True:
        with socket.socket() as s:
            try:
                s.bind(("127.0.0.1", port))
                return port
            except OSError:
                port += 1


def serve(game_dir, port):
    proc = subprocess.Popen(
        [sys.executable, str(game_dir / "tools/serve_live.py"),
         "--port", str(port)],
        cwd=str(game_dir),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(40):
        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{port}/viewer/play.html", timeout=1).read(64)
            return proc
        except Exception:
            time.sleep(0.25)
    proc.kill()
    raise RuntimeError("server never came up")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=4.0)
    ap.add_argument("--only", default=None)
    a = ap.parse_args()
    SHOTS.mkdir(exist_ok=True)

    from playwright.sync_api import sync_playwright

    games = sorted(p for p in PROJECTS.iterdir()
                   if (p / "viewer/play.html").exists())
    if a.only:
        games = [g for g in games if g.name == a.only]

    failures = {}
    with sync_playwright() as pw:
        browser = pw.chromium.launch(headless=True)
        for g in games:
            port = free_port(8300 + games.index(g))
            srv = serve(g, port)
            console_errors, page_errors = [], []
            url = f"http://127.0.0.1:{port}/viewer/play.html"
            try:
                page = browser.new_page(viewport={"width": 1280, "height": 720})
                page.on("console", lambda m: console_errors.append(m.text)
                        if m.type == "error" else None)
                page.on("pageerror", lambda e: page_errors.append(str(e)))
                page.goto(url, wait_until="load", timeout=15000)
                page.wait_for_timeout(int(a.seconds * 1000))
                shot = SHOTS / f"{g.name}.png"
                page.screenshot(path=str(shot))
                # poke the world: walk forward - a live game answers
                page.keyboard.down("KeyW")
                page.wait_for_timeout(900)
                page.keyboard.up("KeyW")
                shot2 = SHOTS / f"{g.name}_t2.png"
                page.wait_for_timeout(300)
                page.screenshot(path=str(shot2))
                # runtime state probe: canvas present? error box clean?
                probe = page.evaluate("""() => ({
                    canvases: document.querySelectorAll('canvas').length,
                    hud: (document.getElementById('hud')||{}).textContent || '',
                    err: (document.getElementById('err')||{}).textContent || '',
                })""")
            finally:
                failures.setdefault(g.name, {}).update(
                    console=console_errors[:4], page=page_errors[:4],
                    probe=locals().get("probe"))
                srv.kill()

        browser.close()

    ok = True
    print(f"{'project':<20} {'errors':<7} {'canvas':<7} {'colors':<7} verdict")
    print("-" * 70)

    def png_render_score(path):
        """Edge density of the screenshot via pygame.

        A blank page (solid bg + HUD box) has near-zero neighbor-pixel
        deltas; any drawn geometry produces strong edges. Returns the
        fraction of horizontal neighbor pairs differing by >12."""
        import pygame
        surf = pygame.image.load(str(path))
        w, h = surf.get_size()
        step = 6
        pairs = edged = 0
        for y in range(70, h - 8, step):        # skip HUD strip
            prev = None
            for x in range(8, w - 8, step):
                c = surf.get_at((x, y))[:3]
                if prev is not None:
                    pairs += 1
                    if (abs(c[0] - prev[0]) + abs(c[1] - prev[1])
                            + abs(c[2] - prev[2]) > 36):
                        edged += 1
                prev = c
        return (edged / pairs) if pairs else 0.0

    def png_frame_diff(path_a, path_b):
        """Mean channel delta between two moments - living games animate."""
        import pygame
        a = pygame.image.load(str(path_a))
        b = pygame.image.load(str(path_b))
        w, h = a.get_size()
        step = 10
        total = n = 0
        for y in range(70, h - 8, step):
            for x in range(8, w - 8, step):
                ca, cb = a.get_at((x, y))[:3], b.get_at((x, y))[:3]
                total += abs(ca[0]-cb[0]) + abs(ca[1]-cb[1]) + abs(ca[2]-cb[2])
                n += 1
        return (total / n) if n else 0.0

    for name, r in failures.items():
        n_err = len(r["console"]) + len(r["page"])
        probe = r.get("probe") or {}
        n_canvas = probe.get("canvases", 0)
        try:
            score = png_render_score(SHOTS / f"{name}.png")
            diff = png_frame_diff(SHOTS / f"{name}.png",
                                  SHOTS / f"{name}_t2.png")
        except Exception:
            score, diff = -1.0, -1.0
        # hidden boot errors surface in the #err box
        if probe.get("err"):
            r["page"].append("errbox: " + probe["err"][:120])
            n_err += 1
        alive = (score > 0.01) or (diff > 1.5)   # geometry OR animation
        verdict = ("OK" if n_err == 0 and n_canvas >= 1 and alive
                   else "FAIL")
        if verdict == "FAIL":
            ok = False
            for e in (r["page"] + r["console"])[:2]:
                print(f"   ! {name}: {e[:110]}")
        print(f"{name:<20} {n_err:<7} {n_canvas:<7} "
              f"e={score:.3f} d={diff:.1f} [{verdict}]")
    print("-" * 70)
    print("screenshots:", SHOTS)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
