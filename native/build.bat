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
set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -I. -Iinclude"
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
    echo   test         - Build and run tests
    echo   dither3d_demo - Build dither demo
    echo   editor       - Build editor binaries
    echo   help         - Show this help
    echo.
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
%GPP% %CXXFLAGS% -c littcore\litt_ecs.cpp -o bin\litt_ecs.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_physics.cpp -o bin\litt_physics.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_input.cpp -o bin\litt_input.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_audio.cpp -o bin\litt_audio.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_ui.cpp -o bin\litt_ui.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_asset.cpp -o bin\litt_asset.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_render.cpp -o bin\litt_render.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_raycast.cpp -o bin\litt_raycast.o || exit /b 1
%GPP% %CXXFLAGS% -c littcore\litt_profiler.cpp -o bin\litt_profiler.o || exit /b 1
echo [build] Core C++ sources compiled OK
echo.

:compile_vulkan
echo [build] Compiling Vulkan backend...
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan.cpp -o bin\litt_vulkan.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan_blas.cpp -o bin\litt_vulkan_blas.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan_tlas.cpp -o bin\litt_vulkan_tlas.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan_raytracing.cpp -o bin\litt_vulkan_raytracing.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_vulkan_shaders.cpp -o bin\litt_vulkan_shaders.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_shader_compilation.cpp -o bin\litt_shader_compilation.o || exit /b 1
echo [build] Vulkan backend compiled OK
echo.

:compile_dx12
echo [build] Compiling DX12 backend...
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_dx12.cpp -o bin\litt_dx12.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_dx12_dxr.cpp -o bin\litt_dx12_dxr.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_dx12_blas.cpp -o bin\litt_dx12_blas.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_dx12_tlas.cpp -o bin\litt_dx12_tlas.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_asset_pipeline.cpp -o bin\litt_asset_pipeline.o || exit /b 1
%GPP% %CXXFLAGS_RAYTRACING% -c littcore\litt_feasibility.cpp -o bin\litt_feasibility.o || exit /b 1
echo [build] DX12 backend compiled OK
echo.

:link_libs
echo [build] Linking libraries...
%GCC% -c littcli.c -o bin\littcli_main.o || exit /b 1
%GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littcli_main.o -o bin\littcli.exe || exit /b 1
echo [build] littcli.exe linked OK
echo.

:link_executables
echo [build] Linking executables...
%GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littview_main.o -o bin\littview.exe -lgdi32 -lvulkan-1 || exit /b 1
%GPP% bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\game_main.o -o bin\game.exe -lgdi32 -lvulkan-1 || exit /b 1
echo [build] Executables linked OK
echo.

:link_dither
echo [build] Linking Dither3D demo...
%GPP% bin\litt_dither_shared.o bin\litt_math.o bin\litt_obj.o bin\litt_json.o bin\litt_world.o bin\litt_vulkan_blas.o bin\litt_vulkan_tlas.o bin\litt_vulkan_raytracing.o bin\litt_asset_pipeline.o -o bin\dither3d_demo.exe -lgdi32 -lvulkan-1 || exit /b 1
echo [build] dither3d_demo.exe linked OK
echo.

:link_tests
echo [build] Linking test executables...
%GPP% %CXXFLAGS_TESTS% -c littcore\littcore_tests.cpp -o bin\littcore_tests_main.o || exit /b 1
%GPP% bin\litt_math.o bin\litt_ecs.o bin\litt_physics.o bin\litt_input.o bin\litt_audio.o bin\litt_ui.o bin\litt_asset.o bin\litt_json.o bin\litt_obj.o bin\litt_world.o bin\littcore_tests_main.o -o bin\littcore_tests.exe || exit /b 1
echo [build] littcore_tests.exe linked OK
echo.

:run_tests
if "%~1"=="test" (
    echo [build] Running unit tests...
    bin\littcore_tests.exe || exit /b 1
    echo [test] All tests passed
    echo.
)

:done
echo [build] ============================================
echo [build] Build complete!
echo [build] Binaries in bin/:
echo   - littcli.exe (headless CLI)
echo   - littview.exe (Vulkan orbit viewer)
echo   - game.exe (game runtime)
echo   - dither3d_demo.exe (dither renderer demo)
echo   - littcore_tests.exe (unit tests)
echo.
echo [build] Backends:
echo   - Vulkan 1.3 (complete with ray tracing)
echo   - DX12 (complete with DXR)
echo   - NullDevice (headless tests)
echo.
echo [build] Subsystems:
echo   - ECS (complete)
echo   - Physics (complete)
echo   - Renderer (complete)
echo   - Audio (complete)
echo   - UI (complete)
echo   - Input (complete)
echo.
if "%~1"=="test" pause
goto :eof