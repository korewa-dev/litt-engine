#!/usr/bin/env python3
"""Deep diagnosis of one game in headless chromium."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from browser_proof import serve
from playwright.sync_api import sync_playwright

g = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Project/ember-depths")
srv = serve(g.resolve(), free := 8390)
try:
    with sync_playwright() as pw:
        b = pw.chromium.launch(headless=True, args=[
            "--use-angle=swiftshader", "--enable-unsafe-swiftshader"])
        page = b.new_page(viewport={"width": 1280, "height": 720})
        msgs = []
        page.on("console", lambda m: msgs.append(f"{m.type}: {m.text[:120]}"))
        page.on("pageerror", lambda e: msgs.append(f"PAGEERROR: {e}"))
        page.goto(f"http://127.0.0.1:{free}/viewer/play.html",
                  wait_until="networkidle", timeout=20000)
        page.wait_for_timeout(4000)
        info = page.evaluate("""() => {
          const c = document.querySelector('canvas');
          const r = { canvases: document.querySelectorAll('canvas').length,
                      three: typeof window.THREE,
                      bodyChildren: document.body.children.length,
                      title: document.title,
                      hud: (document.getElementById('hud')||{}).textContent,
                      errbox: (document.getElementById('err')||{}).textContent };
          return r;
        }""")
        print("INFO:", info)
        print("CONSOLE:")
        for m in msgs[:12]:
            print("  ", m)
        b.close()
finally:
    srv.kill()
