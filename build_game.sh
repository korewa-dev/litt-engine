#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

CC="${CC:-gcc}"
CXX="${CXX:-g++}"
OUT="${OUT:-ash-escape.exe}"
CFLAGS=(-std=c11 -O2 -Wall -Wextra -I alt/src/native/littcore)
CXXFLAGS=(-std=c++17 -O2 -Wall -Wextra -I alt/src/native/littcore)

command -v "$CC" >/dev/null 2>&1 || { echo "C compiler not found: $CC" >&2; exit 1; }
command -v "$CXX" >/dev/null 2>&1 || { echo "C++ compiler not found: $CXX" >&2; exit 1; }

echo "Compiling C sources..."
"$CC" "${CFLAGS[@]}" -c alt/src/native/littcore/litt_obj.c -o litt_obj.o
"$CC" "${CFLAGS[@]}" -c alt/src/native/littcore/litt_json.c -o litt_json.o
"$CC" "${CFLAGS[@]}" -c alt/src/native/littcore/litt_world.c -o litt_world.o

echo "Compiling C++ game..."
"$CXX" "${CXXFLAGS[@]}" -c game_main.cpp -o game_main.o

echo "Linking..."
"$CXX" game_main.o litt_obj.o litt_json.o litt_world.o -o "$OUT" -lgdi32 -luser32 -lwinmm

echo "BUILD OK: $OUT"
