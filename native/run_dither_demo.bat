@echo off
rem Run Dither3D demo
cd /d "%~dp0"
if exist bin\dither3d_demo.exe (
    echo Starting Dither3D Demo...
    bin\dither3d_demo.exe
) else (
    echo Building first...
    call build.bat dither3d_demo
    if exist bin\dither3d_demo.exe (
        echo Starting Dither3D Demo...
        bin\dither3d_demo.exe
    ) else (
        echo Error: dither3d_demo.exe not found
    )
)
