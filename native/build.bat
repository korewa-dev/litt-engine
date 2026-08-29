@echo off
rem Build littcore with all subsystems and ray tracing backends.
rem Usage:
rem   build.bat                    -> build all libs
rem   build.bat test               -> build and run unit tests
rem   build.bat dither3d_demo      -> build dither demo
rem   build.bat cli                -> build littcli only
rem   build.bat editor             -> build editor binaries
rem   build.bat help               -> show this help
setlocal EnableDelayedExpansion
cd /d "%~dp0"
if not exist bin mkdir bin

rem Locate llvm-mingw compilers
set "MINGW64_PATH=C:\Users\roika\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin"
set "MINGW32_PATH=C:\Users\roika\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-i686\bin"
set "GCC=%MINGW64_PATH%\gcc.exe"
set "GPP=%MINGW64_PATH%\g++.exe"
set "CFLAGS=-std=c11 -O2 -Wall -Wextra -I. -Iinclude"
set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -I. -Iinclude -DLITT_NULL_DEVICE=1"
set "CXXFLAGS_RAYTRACING=-std=c++17 -O2 -Wall -Wextra -I. -Iinclude -DLITT_VULKAN=1 -DLITT_DX12=1"
set "CXXFLAGS_TESTS=-std=c++17 -O0 -g -Wall -Wextra -I. -Iinclude -DLITT_NULL_DEVICE=1"

if not exist "%GCC%" (
    echo [build] gcc not found at %GCC%
    echo [build] Installing llvm-mingw: winget install MartinStorsjo.LLVM-MinGW.UCRT
    goto :eof
)

echo [build] Using compiler: %GCC%
echo [build] Target: %MINGW64_PATH%
echo [build] ============================================
echo.

:help
if "%~1"=="help" (
    echo Usage: build.bat [target]
    echo.
    echo Targets:
    echo   all          - Build all binaries (default)
    echo   cli          - Build littcli.exe only
    echo   test         - Build and run unit tests
    echo   dither3d     - Build dither3d demo
    echo   editor       - Build editor (littview, game)
    echo   help         - Show this help
    goto :eof
)

:compile_c
echo [build] Compiling C sources...
%GCC% %CFLAGS% -c littcore\litt_json.c -o bin\litt_json.o || exit /b 1
%GCC% %CFLAGS% -c littcore\litt_obj.c -o bin\litt_obj.o || exit /b 1
%GCC% %CFLAGS% -c littcore\litt_world.c -o bin\litt_world.o || exit /b 1
echo [build] C sources compiled OK
echo.

:compile_cpp
echo [build] Compiling C++ sources...
%GPP% %CXXFLAGS% -c littcore\litt_math.cpp -o bin\litt_math.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_world.cpp -o bin\litt_world.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_dither.cpp -o bin\litt_dither.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_dither_renderer.cpp -o bin\litt_dither_renderer.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_dither_vulkan.cpp -o bin\litt_dither_vulkan.o || exit /b 1
echo [build] Core C++ sources compiled OK
echo.

:compile_vulkan
echo [build] Compiling Vulkan backend...
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan_raytracing.cpp -o bin\litt_vulkan_raytracing.o || exit /b 1
echo [build] Vulkan backend compiled OK
echo.

:compile_dx12
echo [build] Compiling DX12 backend...
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_dx12_dxr.cpp -o bin\litt_dx12_dxr.o || exit /b 1
echo [build] DX12 backend compiled OK
echo.

:compile_shader
echo [build] Compiling shader pipeline...
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_shader_compilation.cpp -o bin\litt_shader_compilation.o || exit /b 1
echo [build] Shader pipeline compiled OK
echo.

:compile_asset
echo [build] Compiling asset pipeline...
%GPP% %CXXFLAGS% -c littcore\litt_asset_pipeline.cpp -o bin\litt_asset_pipeline.o || exit /b 1
echo [build] Asset pipeline compiled OK
echo.

:compile_feasibility
echo [build] Compiling feasibility studies...
%GPP% %CXXFLAGS% -c littcore\litt_feasibility.cpp -o bin\litt_feasibility.o || exit /b 1
echo [build] Feasibility studies compiled OK
echo.

:link_cli
echo [build] Linking littcli...
%GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\litt_math.o bin\litt_vulkan_raytracing.o bin\litt_dx12_dxr.o bin\litt_shader_compilation.o bin\litt_asset_pipeline.o bin\litt_feasibility.o -o bin\littcli.exe -lgdi32 -lvulkan-1 || exit /b 1
echo [build] littcli linked OK
echo.

:link_dither
echo [build] Linking dither3d_demo...
%GPP% bin\litt_dither_shared.o bin\litt_math.o bin\litt_obj.o bin\litt_json.o bin\litt_world.o bin\litt_dither.o bin\litt_dither_renderer.o bin\litt_dither_vulkan.o bin\litt_vulkan_raytracing.o bin\litt_asset_pipeline.o -o bin\dither3d_demo.exe -lgdi32 -lvulkan-1 || exit /b 1
echo [build] dither3d_demo linked OK
echo.

:link_tests
echo [build] Linking unit tests...
%GPP% %CXXFLAGS_TESTS% -c littcore\littcore_tests.cpp -o bin\littcore_tests.o || exit /b 1
%GPP% %CXXFLAGS_TESTS% -c littcore\integration_tests.cpp -o bin\integration_tests.o || exit /b 1
%GPP% %CXXFLAGS_TESTS% -c littcore\asset_tests.cpp -o bin\asset_tests.o || exit /b 1
%GPP% bin\littcore_tests.o bin\integration_tests.o bin\asset_tests.o bin\litt_math.o bin\litt_obj.o bin\litt_json.o bin\litt_world.o -o bin\littcore_tests.exe || exit /b 1
echo [build] Tests linked OK
echo.

:run_tests
echo [build] Running unit tests...
bin\littcore_tests.exe || exit /b 1
echo.

:link_editor
echo [build] Linking editor binaries...
%GPP% %CXXFLAGS% -c littcore\litt_world.cpp -o bin\litt_world_editor.o || exit /b 1
%GPP% bin\litt_world_editor.o bin\litt_math.o bin\litt_obj.o bin\litt_json.o -o bin\littview.exe -lgdi32 || exit /b 1
%GPP% bin\litt_world_editor.o bin\litt_math.o bin\litt_obj.o bin\litt_json.o -o bin\game.exe -lgdi32 || exit /b 1
echo [build] Editor binaries linked OK
echo.

:build_all
call :compile_c
call :compile_cpp
call :compile_vulkan
call :compile_dx12
call :compile_shader
call :compile_asset
call :compile_feasibility
call :link_cli
call :link_dither
call :link_editor
goto :eof

:build_test
call :compile_c
call :compile_cpp
call :compile_vulkan
call :compile_dx12
call :compile_shader
call :compile_asset
call :compile_feasibility
call :link_cli
call :link_dither
call :link_tests
call :run_tests
goto :eof

:build_cli
call :compile_c
call :compile_cpp
call :compile_vulkan
call :compile_dx12
call :compile_shader
call :compile_asset
call :compile_feasibility
call :link_cli
goto :eof

:build_dither
call :compile_c
call :compile_cpp
call :compile_vulkan
call :compile_dx12
call :compile_asset
call :link_dither
goto :eof

:build_editor
call :compile_c
call :compile_cpp
call :link_editor
goto :eof

rem Main
if "%~1"=="test" (
    call :build_test
) else if "%~1"=="cli" (
    call :build_cli
) else if "%~1"=="dither" (
    call :build_dither
) else if "%~1"=="editor" (
    call :build_editor
) else if "%~1"=="help" (
    call :help
) else (
    call :build_all
)

echo.
echo [build] ============================================
echo [build] Build complete!
echo [build] ============================================
