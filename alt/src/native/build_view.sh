#!/bin/bash
set -e
cd "D:/Allgemein/AI Router/litt engine/alt/src/native"
MINGW64_PATH="/c/Users/roika/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20260616-ucrt-x86_64/bin"
GPP="$MINGW64_PATH/g++.exe"
GCC="$MINGW64_PATH/gcc.exe"
CFLAGS="-std=c11 -O2 -Wall -Wextra -I. -Iinclude"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -I. -Iinclude -DLITT_NULL_DEVICE=1"
mkdir -p bin

echo "Compiling C sources..."
"$GCC" $CFLAGS -c littcore/litt_json.c -o bin/litt_json.o
"$GCC" $CFLAGS -c littcore/litt_obj.c -o bin/litt_obj.o
"$GCC" $CFLAGS -c littcore/litt_world.c -o bin/litt_world.o

echo "Compiling C++ sources..."
"$GPP" $CXXFLAGS -c littcore/litt_math.cpp -o bin/litt_math.o
"$GPP" $CXXFLAGS -c littcore/litt_world.cpp -o bin/litt_world_cpp.o
"$GPP" $CXXFLAGS -c littcore/litt_dither.cpp -o bin/litt_dither.o
"$GPP" $CXXFLAGS -c littcore/litt_dither_renderer.cpp -o bin/litt_dither_renderer.o
"$GPP" $CXXFLAGS -c littcore/litt_shader_compilation.cpp -o bin/litt_shader_compilation.o
"$GPP" $CXXFLAGS -c littview.cpp -o bin/littview_main.o

echo "Linking..."
"$GPP" bin/littview_main.o bin/litt_world_cpp.o bin/litt_math.o bin/litt_json.o bin/litt_obj.o bin/litt_world.o bin/litt_dither.o bin/litt_dither_renderer.o bin/litt_shader_compilation.o -o bin/littview.exe -lgdi32 -luser32
echo "BUILD OK"
ls -la bin/littview.exe
