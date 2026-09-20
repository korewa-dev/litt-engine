#!/usr/bin/env bash
# Litt Engine native build wrapper for POSIX hosts.
# Usage: ./build.sh [linux|macos] [debug|release]

set -euo pipefail

PLATFORM="${1:-linux}"
CONFIG="${2:-release}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$PLATFORM" in
    linux|macos)
        ;;
    windows)
        echo "[error] build.sh does not emulate MSVC. Use build.ps1 on Windows." >&2
        exit 2
        ;;
    android)
        echo "[error] Android is not a supported native release target yet." >&2
        exit 2
        ;;
    *)
        echo "[error] unknown platform: $PLATFORM" >&2
        exit 2
        ;;
esac

case "$CONFIG" in
    debug)
        OPT_FLAGS="-O0 -g"
        ;;
    release)
        OPT_FLAGS="-O2"
        ;;
    *)
        echo "[error] unknown configuration: $CONFIG" >&2
        exit 2
        ;;
esac

CC_BIN="${CC:-cc}"
CXX_BIN="${CXX:-c++}"
COMMON_WARN="-Wall -Wextra"
CFLAGS_VALUE="-std=c11 $OPT_FLAGS $COMMON_WARN -I."
CXXFLAGS_VALUE="-std=c++17 $OPT_FLAGS $COMMON_WARN -I."

echo "[build] Litt native $PLATFORM $CONFIG"
make -C "$SCRIPT_DIR" clean
make -C "$SCRIPT_DIR" \
    CC="$CC_BIN" CXX="$CXX_BIN" \
    CFLAGS="$CFLAGS_VALUE" CXXFLAGS="$CXXFLAGS_VALUE" \
    all release-test

echo "[done] supported native tools and tests passed"
