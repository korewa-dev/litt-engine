@echo off
rem Build littcore + littcli + game + tests with gcc (llvm-mingw). Usage:
rem   build.bat          -> build lib objects + littcli.exe + game.exe
rem   build.bat test     -> build and run unit tests
setlocal
cd /d "%~dp0"
if not exist bin mkdir bin

rem Locate llvm-mingw compilers
set "MINGW_PATH=C:\Users\roika\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin"
set "GCC=%MINGW_PATH%\gcc.exe"
set "GPP=%MINGW_PATH%\g++.exe"
set "CFLAGS=-std=c11 -O2 -Wall -Wextra -I."
set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -I."

if not exist "%GCC%" (
    echo [build] gcc not found at %GCC%
    goto :eof
)

%GCC% %CFLAGS% -c littcore\litt_json.c -o bin\litt_json.o || exit /b 1
%GCC% %CFLAGS% -c littcore\litt_obj.c -o bin\litt_obj.o || exit /b 1
%GCC% %CFLAGS% -c littcore\litt_world.c -o bin\litt_world.o || exit /b 1

%GCC% %CFLAGS% -c littcli.c -o bin\littcli_main.o || exit /b 1
%GCC% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littcli_main.o -o bin\littcli.exe || exit /b 1
echo [build] bin\littcli.exe OK

if exist "%GPP%" (
    %GPP% %CXXFLAGS% -c littview.cpp -o bin\littview_main.o || exit /b 1
    %GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littview_main.o -o bin\littview.exe -lgdi32 || exit /b 1
    echo [build] bin\littview.exe OK

    %GPP% %CXXFLAGS% -c game.cpp -o bin\game_main.o || exit /b 1
    %GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\game_main.o -o bin\game.exe -lgdi32 || exit /b 1
    echo [build] bin\game.exe OK
)

if not "%1"=="test" goto :done
%GCC% %CFLAGS% -c tests.c -o bin\tests_main.o || exit /b 1
%GCC% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\tests_main.o -o bin\littcore_tests.exe || exit /b 1
cd bin
littcore_tests.exe
set RC=%ERRORLEVEL%
cd ..
goto :done

rem Dither3D demo
if exist "%GPP%" (
    %GPP% %CXXFLAGS% -c littcore\litt_dither.cpp -o bin\litt_dither.o || exit /b 1
    %GPP% %CXXFLAGS% -c littcore\litt_dither_renderer.cpp -o bin\litt_dither_renderer.o || exit /b 1
    %GPP% %CXXFLAGS% -c dither3d_demo.cpp -o bin\dither3d_main.o || exit /b 1
    %GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\litt_dither.o bin\litt_dither_renderer.o bin\dither3d_main.o -o bin\dither3d_demo.exe -lgdi32 || exit /b 1
    echo [build] bin\dither3d_demo.exe OK
)

:done
endlocal
