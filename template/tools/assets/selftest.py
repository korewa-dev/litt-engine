#!/usr/bin/env python3
"""selftest.py - proves the AI-asset pipeline end-to-end WITHOUT a real
Stable Diffusion install.

1. Spins up a mock A1111-compatible server (in-process thread).
2. Runs gen_texture.py against it -> PNG must land, index registered,
   materials.mtl patched with map_Kd.
3. Runs gen_texture.py with --fallback -> deterministic procedural asset.
4. Runs gen_heightfield.py on the generated texture -> valid OBJ.
5. Verifies budgets (<256 KB) and determinism (same seed => same bytes).

Run:  python template/tools/assets/selftest.py
"""
import base64
import json
import shutil
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "worldgen"))

# 1x1 red PNG, valid base64
TINY_PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==")


class MockA1111(BaseHTTPRequestHandler):
    def log_message(self, *a):  # silence
        pass

    def _json(self, obj, code=200):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/sdapi/v1/options":
            self._json({"sd_model_checkpoint": "mock"})
        elif self.path == "/sdapi/v1/sd-models":
            self._json([{"title": "mock"}])
        else:
            self._json({}, 404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        self.rfile.read(n)
        if self.path == "/sdapi/v1/txt2img":
            self._json({"images": [base64.b64encode(TINY_PNG).decode()]})
        else:
            self._json({}, 404)


def run_tool(script, *args):
    import subprocess
    r = subprocess.run([sys.executable, str(HERE / script), *args],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise AssertionError("%s failed:\n%s\n%s"
                             % (script, r.stdout[-800:], r.stderr[-800:]))
    return r.stdout


def main():
    failures = []

    def check(cond, label):
        print(("PASS  " if cond else "FAIL  ") + label)
        if not cond:
            failures.append(label)

    srv = ThreadingHTTPServer(("127.0.0.1", 0), MockA1111)
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()

    game = ROOT / "Project" / "_asset_selftest"
    shutil.rmtree(game, ignore_errors=True)
    (game / "assets" / "models").mkdir(parents=True, exist_ok=True)

    try:
        out = run_tool("gen_texture.py",
                       "--game-dir", str(game), "--name", "crypt_stone",
                       "--prompt", "gothic stone wall", "--seed", "5",
                       "--server", "http://127.0.0.1:%d" % port,
                       "--mtl", "paving,crypt_wall")
        tex = game / "assets" / "textures" / "crypt_stone.png"
        check(tex.exists() and len(tex.read_bytes()) > 0, "SD-path PNG written")
        check("stable-diffusion" in out, "source reported as stable-diffusion")

        idx = json.loads((game / "assets" / "asset_index.json").read_text())
        ids = [a["id"] for a in idx["assets"]]
        check("crypt_stone" in ids, "asset_index registration")
        kinds = {a["id"]: a.get("type") for a in idx["assets"]}
        check(kinds.get("crypt_stone") == "texture", "index kind=texture")

        mtl = (game / "assets" / "models" / "materials.mtl")
        mtxt = mtl.read_text(encoding="utf-8") if mtl.exists() else ""
        check("map_Kd ../textures/crypt_stone.png" in mtxt,
              "MTL map_Kd bound for paving/crypt_wall")

        out2 = run_tool("gen_texture.py",
                        "--game-dir", str(game), "--name", "fallback_noise",
                        "--prompt", "ash and cinder", "--seed", "9",
                        "--fallback")
        ftex = game / "assets" / "textures" / "fallback_noise.png"
        check(ftex.exists(), "procedural fallback PNG written")
        check("procedural-fallback" in out2, "fallback source labeled")

        budget_ok = all(len((game / "assets" / "textures" / f).read_bytes())
                        <= 256 * 1024
                        for f in ("crypt_stone.png", "fallback_noise.png"))
        check(budget_ok, "texture budget <256 KB enforced")

        run_tool("gen_heightfield.py",
                 "--game-dir", str(game),
                 "--image", "textures/fallback_noise.png",
                 "--name", "noise_terrain", "--res", "32")
        terrain = game / "assets" / "models" / "noise_terrain.obj"
        ttext = terrain.read_text(encoding="utf-8")
        faces = sum(1 for ln in ttext.splitlines() if ln.startswith("f "))
        check(faces == 32 * 32 * 2, "heightfield OBJ face count (2048)")
        check("v " in ttext, "heightfield has vertices")

        # determinism: same fallback seed twice => identical bytes
        before = (game / "assets" / "textures" / "fallback_noise.png").read_bytes()
        run_tool("gen_texture.py", "--game-dir", str(game),
                 "--name", "fallback_noise", "--prompt", "ash and cinder",
                 "--seed", "9", "--fallback")
        after = (game / "assets" / "textures" / "fallback_noise.png").read_bytes()
        check(before == after, "deterministic fallback (same seed => same bytes)")
    finally:
        srv.shutdown()
        shutil.rmtree(game, ignore_errors=True)

    print("\n%s: %d checks failed" % ("SELFTEST FAILED" if failures else
                                      "SELFTEST OK", len(failures)))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
