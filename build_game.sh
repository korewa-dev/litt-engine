#!/bin/bash
set -e
cd "D:/Allgemein/AI Router/litt engine"
MINGW64_PATH="/c/Users/roika/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20260616-ucrt-x86_64/bin"
GPP="$MINGW64_PATH/g++.exe"
GCC="$MINGW64_PATH/gcc.exe"
CFLAGS="-std=c11 -O2 -Wall -Wextra -I alt/src/native/littcore"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -I alt/src/native/littcore"

echo "Compiling C sources..."
"$GCC" $CFLAGS -c alt/src/native/littcore/litt_obj.c -o litt_obj.o
"$GCC" $CFLAGS -c alt/src/native/littcore/litt_json.c -o litt_json.o
"$GCC" $CFLAGS -c alt/src/native/littcore/litt_world.c -o litt_world.o

echo "Compiling C++ game..."
"$GPP" $CXXFLAGS -c game_main.cpp -o game_main.o

echo "Linking..."
"$GPP" game_main.o litt_obj.o litt_json.o litt_world.o -o ash-escape.exe -lgdi32 -luser32 -lwinmm
echo "BUILD OK"
ls -la ash-escape.exe
