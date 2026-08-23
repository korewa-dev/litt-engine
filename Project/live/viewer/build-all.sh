#!/usr/bin/env bash
# build-all.sh - UNIVERSAL builder for the Litt live viewer.
# Detects its environment and produces the right binary:
#   android (Termux) -> live-android     linux/macos -> live
#   windows (git-bash/msys/wsl) -> live.exe
#
# Requirements: rust/cargo.
#   linux/mac : curl https://sh.rustup.rs -sSf | sh
#   termux    : pkg install rust
#   windows   : https://rustup.rs  (run from git-bash, or use build-all.ps1)
#
# Optional cross-builds: ./build-all.sh cross-windows  (needs mingw-w64)
#                         see build-all.ps1 for android NDK route.

set -e
cd "$(dirname "$0")"

# ---- locate cargo ----------------------------------------------------------
if ! command -v cargo >/dev/null 2>&1; then
  [ -f "$HOME/.cargo/bin/cargo" ] && export PATH="$HOME/.cargo/bin:$PATH"
fi
if ! command -v cargo >/dev/null 2>&1; then
  echo "[fail] cargo not found."
echo "  linux/mac : curl https://sh.rustup.rs -sSf | sh"
echo "  termux    : pkg install rust"
echo "  windows   : install rustup, or use .\build-all.ps1 instead"
  exit 1
fi

# ---- detect environment ----------------------------------------------------
u="$(uname -s 2>/dev/null || echo unknown)"
if [ -n "${TERMUX_VERSION:-}" ] || [ -n "${TERMUX_APP__PACKAGE_NAME:-}" ]; then
  ENV=android
else
  case "$u" in
    *Darwin*)             ENV=macos ;;
    *Linux*)              ENV=linux  ;;
    MINGW*|MSYS*|CYGWIN*) ENV=windows ;;
    *)                    ENV=unknown ;;
  esac
fi
echo "[..] environment: $ENV ($u)"

# ---- choose flags ----------------------------------------------------------
# The engine repo pins target=x86_64-pc-windows-gnu via its root .cargo/config.toml.
# If mingw is missing on this Windows machine, explicitly switch to MSVC instead.
TARGET_FLAG=""
if [ "$ENV" = windows ]; then
  if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1 \
     && rustup target list --installed 2>/dev/null | grep -q x86_64-pc-windows-msvc; then
    TARGET_FLAG="--target x86_64-pc-windows-msvc"
    echo "[..] mingw linker absent -> building for msvc"
  fi
fi

# ---- build ------------------------------------------------------------------
# shellcheck disable=SC2086
cargo build --release $TARGET_FLAG

# ---- locate produced binary (target layout varies by pinned config) --------
BIN=""
for c in \
  target/x86_64-pc-windows-gnu/release/live.exe \
  target/x86_64-pc-windows-msvc/release/live.exe \
  target/release/live.exe \
  target/release/live ;
do
  if [ -f "$c" ]; then BIN="$c"; break; fi
done
if [ -z "$BIN" ]; then echo "[fail] built binary not found under target/"; exit 1; fi

case "$ENV" in
  android) OUT="live-android" ;;
  windows) OUT="live.exe" ;;
  *)       OUT="live" ;;
esac
cp "$BIN" "$OUT"
chmod +x "$OUT" 2>/dev/null || true
echo "[ok] ./$OUT ($(du -h "$OUT" | cut -f1))"
echo "run:  ./$OUT            # serves http://127.0.0.1:8088/viewer/"
echo "      ./$OUT --help     # port/bind/root options"
