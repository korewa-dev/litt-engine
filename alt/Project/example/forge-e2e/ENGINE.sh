#!/bin/sh
# Litt native launcher - engine plays this world in its own window
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
EXE="$LITT_ENGINE"
[ -n "$EXE" ] || EXE="$ROOT/native/bin/littview"
[ -x "$EXE" ] || EXE="$ROOT/native/bin/littview"
exec "$EXE" window "$HERE"
