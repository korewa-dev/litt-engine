#!/usr/bin/env python3
"""Litt Live - read-only live server + visualization host.

Serves the Project/live directory so any browser on the network can watch
the AI build the world in real time.

READ-ONLY BY DESIGN: only GET is implemented. There is no endpoint through
which a connected human or tool could modify anything - mutations belong
to the AI working on disk, never to viewers.

Usage:
  python serve_live.py [--port 8088] [--bind 127.0.0.1]

Then open:  http://127.0.0.1:<port>/viewer/
Expose with --bind 0.0.0.0 to let other users connect (restrict access via
firewall / reverse-proxy auth as documented in README.md).
"""
import argparse
import http.server
import os
from pathlib import Path

LIVE_DIR = Path(__file__).resolve().parent.parent

class ReadOnlyHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(LIVE_DIR), **kwargs)

    def do_GET(self):
        if self.path.startswith("/viewer") or self.path == "/":
            self.path = "/viewer/index.html"
        super().do_GET()   # SimpleHTTPRequestHandler emits the full 200 itself

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        http.server.SimpleHTTPRequestHandler.end_headers(self)

    def _deny(self):
        self.send_error(405, "Litt Live is AI-exclusive: read-only observer")

    do_POST = do_PUT = do_PATCH = do_DELETE = _deny

    def log_message(self, fmt, *args):
        print("[serve] " + (fmt % args))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8088)
    ap.add_argument("--bind", default="127.0.0.1")
    a = ap.parse_args()
    try:
        srv = http.server.ThreadingHTTPServer((a.bind, a.port), ReadOnlyHandler)
    except OSError as e:
        raise SystemExit(
            "PORT %d BUSY (%s). Another serve_live may already run - possibly\n"
            "from a DIFFERENT checkout (e.g. a Downloads zip copy). Find it:\n"
            "  Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | Select ProcessId, CommandLine\n"
            "then stop it before starting here." % (a.port, e))
    print("LITT LIVE - read-only observer server")
    print("  serving directory: %s" % LIVE_DIR)
    print("  local:   http://127.0.0.1:%d/viewer/" % a.port)
    if a.bind == "0.0.0.0":
        print("  network: http://<this-pc>:%d/viewer/   (share at your own risk)" % a.port)
    print("  humans observe; only the AI modifies the world.")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("server stopped")

if __name__ == "__main__":
    main()
