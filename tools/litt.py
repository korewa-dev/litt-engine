#!/usr/bin/env python3
"""litt - one command to cook, serve and eat the whole engine pie.

    litt            status dashboard (games, builds, health)
    litt build      [--full]  native C/C++ always; Rust too when --full
    litt test                 C unit tests + viewer selftest + project audit
    litt proof                full native render+sim proof over shipped games
    litt new NAME   [...]     generate a new game (args pass to make_game)
    litt play GAME            play a game in the Vulkan player
    litt view GAME            open the C++ orbit viewer
    litt bench [GAME]         rasterizer benchmark
    litt studio               build+launch the C# Litt Studio GUI
    litt doctor               toolchain health check

Thin launchers: litt.bat (Windows) / litt (POSIX) call this script.
"""
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
NATIVE = REPO / "native"
PROJECTS = REPO / "Project"
IS_WIN = os.name == "nt"


def exe(name):
    """native/bin/<name> with .exe suffix when needed"""
    p = NATIVE / "bin" / (name + (".exe" if IS_WIN else ""))
    return p if p.exists() else None


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], **kw)


def sh(cmd):
    """convenience: shell line -> CompletedAttempt"""
    return run(cmd, capture_output=True, text=True)


# ------------------------------------------------------------------ verbs
def cmd_status():
    print("LITT ENGINE - %s" % REPO.name)
    print("=" * 64)
    cli = exe("littcli")
    view = exe("littview")
    print("native core : %s" % ("built" if cli and view else "MISSING - run: litt build"))
    eng = None
    for cand in ([REPO / "target/x86_64-pc-windows-gnu/release/litt.exe"] if IS_WIN else []) \
            + [REPO / "target/release/litt", REPO / "target/debug/litt"]:
        if Path(str(cand) + (".exe" if IS_WIN else "")).exists():
            eng = cand
            break
    print("engine (Rust): %s" % (eng or "not built (optional; 'litt build --full')"))
    games = sorted(p for p in PROJECTS.iterdir()
                   if p.is_dir() and (p / "world_state.json").exists())
    print("games       : %d (%d shippable)\n" % (
        len(games),
        sum(1 for g in games if (g / "story").is_dir())))
    print("%-18s %-6s %-8s %-9s %s" % ("GAME", "SHIP", "MODE", "ENTITIES", "LAUNCH"))
    print("-" * 64)
    for g in games:
        mode = ent = "?"
        if cli:
            r = sh([cli, "validate", g, "--frames", "10"])
            try:
                js = __import__("json").loads(
                    r.stdout.strip().splitlines()[-1])
                mode = js.get("mode", "?")
                ent = str(js.get("interactives", "?"))
                if not js.get("ok"):
                    mode = "BROKEN"
            except Exception:
                mode = "no-sim"
        ship = "yes" if (g / "story").is_dir() else ""
        launch = "litt play " + g.name
        print("%-18s %-6s %-8s %-9s %s" % (g.name, ship, mode, ent, launch))
    return 0


def cmd_build(full=False):
    print("[1/2] native C/C++ core...")
    if IS_WIN:
        r = run(["cmd", "/c", str(NATIVE / "build.bat")])
    else:
        r = run(["make", "-C", str(NATIVE)])
    if r.returncode:
        print(r.stdout, r.stderr)
        return r.returncode
    if full:
        cargo = shutil.which("cargo")
        if not cargo:
            print("cargo not found - skipping Rust engine "
                  "(install https://rustup.rs for the Vulkan player)")
            return 0
        print("[2/2] Rust engine (--release)...")
        r = run([cargo, "build", "--release"], cwd=REPO)
        return r.returncode
    print("(Rust engine skipped - use 'litt build --full' for it)")
    return 0


def cmd_test():
    fails = []
    print("== C unit tests ==")
    if IS_WIN:
        r = run(["cmd", "/c", "%s test" % (NATIVE / "build.bat")])
        if r.returncode:
            fails.append("c-tests")
    else:
        r = run(["make", "-C", str(NATIVE), "test"])
        if r.returncode:
            fails.append("c-tests")
    print("== littview selftest ==")
    v = exe("littview")
    if v:
        r = sh([v, "selftest"])
        print(r.stdout.strip() or r.stderr.strip())
        if r.returncode:
            fails.append("selftest")
    else:
        print("SKIP (not built)")
    print("== project audit ==")
    r = sh([sys.executable, REPO / "template/tools/assets/verify_project.py"])
    tail = "\n".join(r.stdout.strip().splitlines()[-4:])
    print(tail)
    if r.returncode:
        fails.append("audit")
    print("\n%s" % ("ALL GREEN" if not fails else "FAILED: " + ", ".join(fails)))
    return 1 if fails else 0


def cmd_proof():
    return run([sys.executable,
                REPO / "template/tools/assets/native_proof.py"]).returncode


def cmd_new(args):
    script = REPO / "template/tools/worldgen/make_game.py"
    return run([sys.executable, script] + args).returncode


def _game_dir(name):
    g = PROJECTS / name
    if not (g / "world_state.json").exists():
        print("no such game: %s (see: litt)" % name, file=sys.stderr)
        sys.exit(2)
    return g


def _engine_exe():
    """the Rust Vulkan player if built, else None"""
    cands = []
    if IS_WIN:
        cands += [REPO / "target/x86_64-pc-windows-gnu/release/litt.exe",
                  REPO / "target/x86_64-pc-windows-gnu/debug/litt.exe"]
    cands += [REPO / "target/release/litt", REPO / "target/debug/litt"]
    for c in cands:
        p = Path(str(c) + (".exe" if IS_WIN else ""))
        if p.exists():
            return p
    return None


def cmd_play(name):
    g = _game_dir(name)
    eng = _engine_exe()
    if not eng:
        print("Rust player not built (litt build --full) - "
              "opening the C++ viewer instead")
        return cmd_view([name])
    # ENGINE launchers resolve $LITT_ENGINE/release/debug themselves;
    # detach so the CLI returns
    if IS_WIN:
        subprocess.Popen(["cmd", "/c", "start", "", str(g / "ENGINE.bat")],
                         cwd=str(g), creationflags=0x00000008,
                         close_fds=False)
        return 0
    return run(["sh", str(g / "ENGINE.sh")]).returncode


def cmd_view(args):
    g = _game_dir(args[0])
    v = exe("littview")
    if not v:
        print("littview not built - run: litt build", file=sys.stderr)
        return 2
    # --shot NAME.bmp -> offscreen render instead of the live window
    if "--shot" in args:
        i = args.index("--shot")
        out = args[i + 1] if i + 1 < len(args) else "frame.bmp"
        return run([v, "render", g, "--out", out]).returncode
    # live window: detached so the CLI returns immediately
    flags = 0x00000008 if IS_WIN else 0  # DETACHED_PROCESS
    subprocess.Popen([str(v), "window", str(g)],
                     creationflags=flags, close_fds=False)
    print("[view] littview window opened for %s" % name)
    return 0


def cmd_bench(args):
    v = exe("littview")
    if not v:
        print("littview not built - run: litt build", file=sys.stderr)
        return 2
    game = args[0] if args else "drowned-vow-42"
    r = sh([v, "bench", PROJECTS / game, "--frames", "200"])
    print(r.stdout.strip() or r.stderr.strip())
    return r.returncode


def cmd_studio():
    """build (if stale) + launch the C# studio, detached"""
    exe_path = REPO / "studio" / "LittStudio.exe"
    src = REPO / "studio" / "cs" / "LittStudio.cs"
    if not exe_path.exists() or src.stat().st_mtime > exe_path.stat().st_mtime:
        print("[studio] building...")
        r = run(["cmd", "/c", str(REPO / "tools/build-studio.bat")])
        if r.returncode:
            return r.returncode
    flags = 0x00000008 if IS_WIN else 0  # DETACHED_PROCESS on Windows
    subprocess.Popen([str(exe_path)], cwd=str(REPO),
                     creationflags=flags, close_fds=False)
    print("[studio] launched - look for the LITT STUDIO window")
    return 0


def _find_csc():
    """Locate Roslyn csc: CSC env var, then vswhere (official probe),
    then well-known drive roots. Mirrors tools/build-studio.bat."""
    env = os.environ.get("CSC")
    if env and Path(env).exists():
        return env
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = vswhere / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        try:
            out = subprocess.run(
                [str(vswhere), "-latest", "-requires", "Microsoft.Component.MSBuild",
                 "-find", r"MSBuild\**\Bin\MSBuild.exe"],
                capture_output=True, text=True, timeout=20).stdout.strip()
            if out:
                msb = Path(out.splitlines()[0])
                csc = msb.parent / "Roslyn" / "csc.exe"
                if csc.exists():
                    return str(csc)
        except Exception:
            pass
    for d in ("D:", "C:"):
        c = Path(d + r"\Program Files\Program\MSBuild\Current\Bin\Roslyn\csc.exe")
        if c.exists():
            return str(c)
    return None


def cmd_doctor():
    checks = [
        ("python", shutil.which("python") or shutil.which("python3")),
        ("gcc", shutil.which("gcc")),
        ("g++", shutil.which("g++")),
        ("cc/make", shutil.which("make") or shutil.which("nmake")),
        ("cargo (rust player)", shutil.which("cargo")),
        ("csc (C# studio)", _find_csc()),
        ("git", shutil.which("git")),
    ]
    print("%-24s %s" % ("TOOL", "STATUS"))
    print("-" * 48)
    for name, path in checks:
        if path:
            print("%-24s OK   %s" % (name, path))
        else:
            need = {
                "cargo (rust player)": "install via https://rustup.rs",
            }.get(name, "")
            print("%-24s --   %s" % (name, need))
    cli, view = exe("littcli"), exe("littview")
    print("%-24s %s" % ("native core",
                        "OK   built" if cli and view else "MISSING - litt build"))
    print("\nplatform: %s %s" % (platform.system(), platform.release()))
    return 0


# ------------------------------------------------------------------ main
def main():
    argv = sys.argv[1:]
    verb = argv[0] if argv else "status"
    rest = argv[1:]
    table = {
        "status": lambda: cmd_status(),
        "build": lambda: cmd_build("--full" in rest or "-f" in rest),
        "test": lambda: cmd_test(),
        "proof": lambda: cmd_proof(),
        "new": lambda: cmd_new(rest),
        "play": lambda: cmd_play(rest[0]),
        "view": lambda: cmd_view(rest),
        "bench": lambda: cmd_bench(rest),
        "studio": lambda: cmd_studio(),
        "doctor": lambda: cmd_doctor(),
        "help": lambda: main.__globals__["_usage"](),
    }
    fn = table.get(verb)
    if not fn:
        _usage()
        return 2
    return fn()


def _usage():
    print(__doc__.strip())


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
