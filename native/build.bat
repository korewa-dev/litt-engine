@echo off
rem Build littcore + littcli + game + tests with gcc (llvm-mingw). Usage:
rem   build.bat          -> build lib objects + littcli.exe + game.exe
rem   build.bat test     -> build and run unit tests
setlocal
cd /d "%~dp0"
if not exist bin mkdir bin
set CC=gcc
set CFLAGS=-std=c11 -O2 -Wall -Wextra -I.

%CC% %CFLAGS% -c littcore\litt_json.c -o bin\litt_json.o || exit /b 1
%CC% %CFLAGS% -c littcore\litt_obj.c -o bin\litt_obj.o || exit /b 1
%CC% %CFLAGS% -c littcore\litt_world.c -o bin\litt_world.o || exit /b 1

%CC% %CFLAGS% -c littcli.c -o bin\littcli_main.o || exit /b 1
%CC% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littcli_main.o -o bin\littcli.exe || exit /b 1
echo [build] bin\littcli.exe OK

rem Stage-2 C++ front-end (needs g++)
where g++ >nul 2>nul && (
    g++ -std=c++17 -O2 -Wall -Wextra -I. -c littview.cpp -o bin\littview_main.o || exit /b 1
    g++ bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littview_main.o -o bin\littview.exe -lgdi32 || exit /b 1
    echo [build] bin\littview.exe OK
)

rem Game player binary (needs g++, optional if not present)
where g++ >nul 2>nul && (
    g++ -std=c++17 -O2 -Wall -Wextra -I. -c game.cpp -o bin\game_main.o || exit /b 1
    g++ bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\game_main.o -o bin\game.exe -lgdi32 || exit /b 1
    echo [build] bin\game.exe OK
)

if not "%1"=="test" goto :done
%CC% %CFLAGS% -c tests.c -o bin\tests_main.o || exit /b 1
%CC% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\tests_main.o -o bin\littcore_tests.exe || exit /b 1
cd bin
littcore_tests.exe
set RC=%ERRORLEVEL%
cd ..
exit /b %RC%
:done
