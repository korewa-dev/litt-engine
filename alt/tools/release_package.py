#!/usr/bin/env python3
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

FORBIDDEN_SUFFIXES = (".o", ".o.tmp", ".exe.tmp")
FORMAT_VERSION = 1

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def git_sha(root: Path) -> str:
    return subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()

def add_deterministic(tar: tarfile.TarFile, source: Path, arcname: str) -> None:
    info = tar.gettarinfo(str(source), arcname)
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mtime = 0
    if source.is_file():
        with source.open("rb") as fh:
            tar.addfile(info, fh)
    else:
        tar.addfile(info)

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--dist", default="alt/src/native/dist")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    dist = (root / args.dist).resolve()
    out = Path(args.out).resolve()
    include = dist / "include" / "litt"
    library = dist / "lib" / "liblittcore.a"
    license_file = root / "LICENSE"

    if not include.is_dir() or not library.is_file() or not license_file.is_file():
        raise SystemExit("C++ SDK dist, static library, or LICENSE is missing")

    files = sorted(p for p in include.rglob("*") if p.is_file())
    files.append(library)
    files.append(license_file)

    for path in files:
        if path.name.endswith(FORBIDDEN_SUFFIXES):
            raise SystemExit(f"forbidden compiler product in package: {path}")

    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="litt-release-") as temp:
        stage = Path(temp) / "litt-sdk"
        (stage / "include" / "litt").mkdir(parents=True)
        (stage / "lib").mkdir(parents=True)

        for header in sorted(include.iterdir()):
            if header.is_file():
                shutil.copyfile(header, stage / "include" / "litt" / header.name)
        shutil.copyfile(library, stage / "lib" / library.name)
        shutil.copyfile(license_file, stage / "LICENSE")

        manifest = {
            "format_version": FORMAT_VERSION,
            "source_sha": git_sha(root),
            "language": "C++17",
            "library": "lib/liblittcore.a",
            "include_root": "include/litt",
        }
        (stage / "manifest.json").write_text(
            json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8"
        )

        checksums = []
        staged_files = sorted(p for p in stage.rglob("*") if p.is_file())
        for path in staged_files:
            rel = path.relative_to(stage).as_posix()
            if rel == "SHA256SUMS":
                continue
            checksums.append(f"{sha256(path)}  {rel}")
        (stage / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")

        with out.open("wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
                with tarfile.open(fileobj=gz, mode="w") as tar:
                    for path in sorted(stage.rglob("*"), key=lambda p: p.relative_to(stage).as_posix()):
                        add_deterministic(tar, path, f"litt-sdk/{path.relative_to(stage).as_posix()}")

    print(f"{sha256(out)}  {out.name}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
