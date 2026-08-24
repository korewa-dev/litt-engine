// Build script for Litt Engine - lightweight, cross-platform
# Usage: build.sh [linux|windows|android] [debug|release]

#!/bin/bash
set -e

PLATFORM="${1:-linux}"
CONFIG="${2:-release}"
CFLAGS="-std=c11 -O2 -Wall"
CXXFLAGS="-std=c++17 -O2 -Wall"

case "$PLATFORM" in
    windows) CFLAGS+=" -DWIN32"; CXXFLAGS+=" -DWIN32"; LIBS="-lgdi32 -lcomctl32" ;;
    linux) CFLAGS+=" -DLINUX"; CXXFLAGS+=" -DLINUX"; LIBS="-lX11 -lvulkan-1 -ldl" ;;
    android) CFLAGS+=" -DANDROID -fPIC"; CXXFLAGS+=" -DANDROID -fPIC"; LIBS="-landroid -lEGL -lGLESv3" ;;
esac

[ "$CONFIG" = "debug" ] && CFLAGS+=" -g -O0" && CXXFLAGS+=" -g -O0"

BINDIR="bin/$PLATFORM/$CONFIG"
mkdir -p "$BINDIR"

echo "[build] $PLATFORM $CONFIG"

# Compile C
for f in littcore/*.c; do
    [ -f "$f" ] && $CC $CFLAGS -c "$f" -o "$BINDIR/$(basename $f .c).o" && echo "  [ok] $(basename $f)"
done

# Compile C++
for f in littcore/*.cpp; do
    [ -f "$f" ] && $CXX $CXXFLAGS -c "$f" -o "$BINDIR/$(basename $f .cpp).o" && echo "  [ok] $(basename $f)"
done

# Link
$CC "$BINDIR"/*.o -o "$BINDIR/littcli" $LIBS 2>/dev/null && echo "  [ok] littcli"
$CXX "$BINDIR"/*.o game.cpp -o "$BINDIR/game" $LIBS 2>/dev/null && echo "  [ok] game"
$CXX "$BINDIR"/*.o litteditor.cpp -o "$BINDIR/LittEditor" $LIBS 2>/dev/null && echo "  [ok] LittEditor"

echo "[done] bin/$PLATFORM/$CONFIG/"
