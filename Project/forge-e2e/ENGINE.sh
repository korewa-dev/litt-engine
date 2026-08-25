#!/bin/sh
# Litt native launcher - engine plays this world in its own window
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
EXE="$LITT_ENGINE"
[ -n "$EXE" ] || EXE="$ROOT/target/x86_64-pc-windows-gnu/release/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/x86_64-pc-windows-gnu/debug/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/release/litt"
[ -x "$EXE" ] || EXE="$ROOT/target/debug/litt"
exec "$EXE" play "$HERE"
