@echo off
rem Build littcore + littcli + tests with gcc (llvm-mingw). Usage:
rem   build.bat          -> build lib objects + littcli.exe
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

if "%1"=="test" (
    %CC% %CFLAGS% -c tests.c -o bin\tests_main.o || exit /b 1
    %CC% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\tests_main.o -o bin\littcore_tests.exe || exit /b 1
    cd bin && littcore_tests.exe
    exit /b %ERRORLEVEL%
)
