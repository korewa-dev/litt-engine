#!/usr/bin/env python3
"""litt - one command to cook, serve and eat the whole engine pie.

    litt            status dashboard (games, builds, health)
    litt build                native C/C++ core only (no other toolchain)
    litt test                 C unit tests + viewer selftest + project audit
    litt proof                full native render+sim proof over shipped games
    litt new NAME   [...]     generate a new game (args pass to make_game)
    litt forge "PHRASE" [...] plan+compose a multi-region WorldForge world
                              (--seed S --name N --regions K feed the planner,
                               --out-dir D --force --skip-native-proof the
                               composer; see CDR-011)
    litt refine [...]         generate->prove->refine loop (args pass to
                              refine_game.py, e.g. --kind space --base-seed 42)
    litt play GAME            play a game via its ENGINE launcher
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
WORLDFORGE = REPO / "template" / "tools" / "worldgen"
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


def cmd_build():
    print("[1/1] native C/C++ core...")
    if IS_WIN:
        r = run(["cmd", "/c", str(NATIVE / "build.bat")])
    else:
        r = run(["make", "-C", str(NATIVE)])
    if r.returncode:
        print(r.stdout, r.stderr)
    return r.returncode


def cmd_test():
    fails = []
    print("== C unit tests ==")
    if IS_WIN:
        # invoke the .bat directly (arg-list form): survives repo paths
        # containing spaces; going through `cmd /c` mangles the quoting
        r = run([str(NATIVE / "build.bat"), "test"])
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


# ------------------------------------------------------------- worldforge
def _stream(cmd):
    """run cmd, streaming its output live; return (returncode, captured)"""
    p = subprocess.Popen([str(c) for c in cmd], stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True,
                         encoding="utf-8", errors="replace")
    lines = []
    for ln in p.stdout:
        sys.stdout.write(ln)
        lines.append(ln)
    p.wait()
    return p.returncode, "".join(lines)


def _plan_flags(args):
    """split litt forge args -> (about words, planner flags, composer flags).

    Defensive against the documented CLI only (CDR-011): planner takes
    --about/--seed/--name/--regions/--out, composer takes a spec path plus
    --out-dir/--force/--skip-native-proof. Unknown words join the phrase.
    """
    about, plan_args, comp_args = [], [], []
    i = 0
    while i < len(args):
        a = args[i]
        nxt = args[i + 1] if i + 1 < len(args) else None
        if a in ("--seed", "--name", "--regions", "--out") and nxt:
            plan_args += [a, nxt]
            i += 2
        elif a == "--out-dir" and nxt:
            comp_args += [a, nxt]
            i += 2
        elif a in ("--force", "--skip-native-proof"):
            comp_args += [a]
            i += 1
        else:
            about.append(a)
            i += 1
    return about, plan_args, comp_args


def _find_spec(plan_out, plan_args):
    """locate the spec the planner just wrote: --out wins, then its last-JSON
    stdout hint, then the conventional default paths."""
    if "--out" in plan_args:
        p = Path(plan_args[plan_args.index("--out") + 1])
        return p if p.exists() else None
    tail = plan_out.strip().splitlines()
    if tail:
        try:  # planners may print {.. "spec": ..} as their last line
            js = __import__("json").loads(tail[-1])
            for k in ("spec", "spec_path", "world_spec", "out", "path"):
                v = js.get(k) if isinstance(js, dict) else None
                if isinstance(v, str) and Path(v).exists():
                    return Path(v)
        except Exception:
            pass
    here = Path.cwd()
    for cand in (here / "world_spec.json", REPO / "world_spec.json",
                 WORLDFORGE / "world_spec.json"):
        if cand.exists():
            return cand
    return None


def cmd_forge(args):
    """WorldForge (CDR-011): one phrase -> planner spec -> fused world."""
    plan = WORLDFORGE / "world_planner.py"
    comp = WORLDFORGE / "world_forge.py"
    missing = [p.name for p in (plan, comp) if not p.exists()]
    if missing:
        print("worldforge landing shortly - missing %s" % ", ".join(missing),
              file=sys.stderr)
        return 2
    about, plan_args, comp_args = _plan_flags(args)
    if not about:
        print('usage: litt forge "one line about the world" '
              '[--seed S] [--name N] [--regions K] [--out-dir D]',
              file=sys.stderr)
        return 2
    print('[forge 1/2] planning spec for "%s"' % " ".join(about))
    code, out = _stream([sys.executable, plan, "--about",
                         " ".join(about)] + plan_args)
    if code:
        print("[forge] planner failed (%d)" % code, file=sys.stderr)
        return code or 1
    spec = _find_spec(out, plan_args)
    if not spec:
        print("[forge] no world_spec.json found after planning "
              "(pin it with --out PATH)", file=sys.stderr)
        return 1
    print("[forge 2/2] composing fused world from %s" % spec)
    code, _ = _stream([sys.executable, comp, str(spec)] + comp_args)
    if code:
        print("[forge] composer failed (%d)" % code, file=sys.stderr)
        return code or 1
    return 0


def cmd_refine(args):
    """refine loop (CDR-010): pass-through to refine_game.py."""
    script = WORLDFORGE / "refine_game.py"
    if not script.exists():
        print("refine loop landing shortly", file=sys.stderr)
        return 2
    return run([sys.executable, script] + args).returncode


def _game_dir(name):
    g = PROJECTS / name
    if not (g / "world_state.json").exists():
        print("no such game: %s (see: litt)" % name, file=sys.stderr)
        sys.exit(2)
    return g


def cmd_play(name):
    g = _game_dir(name)
    # ENGINE launchers resolve the player binary themselves;
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
        ("csc (C# studio)", _find_csc()),
        ("git", shutil.which("git")),
    ]
    print("%-24s %s" % ("TOOL", "STATUS"))
    print("-" * 48)
    for name, path in checks:
        if path:
            print("%-24s OK   %s" % (name, path))
        else:
            print("%-24s --   %s" % (name, ""))
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
        "build": lambda: cmd_build(),
        "test": lambda: cmd_test(),
        "proof": lambda: cmd_proof(),
        "new": lambda: cmd_new(rest),
        "forge": lambda: cmd_forge(rest),
        "refine": lambda: cmd_refine(rest),
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
