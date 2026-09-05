@echo off
rem build-studio.bat - compile the C# Litt Studio.
rem Primary path : VS-bundled MSBuild + LittStudio.csproj (documented
rem                Microsoft.Common.CurrentVersion.targets machinery,
rem                FrameworkPathOverride covers missing ref assemblies).
rem Fallback path: raw Roslyn csc against the machine's framework dirs.
setlocal
cd /d "%~dp0.."

rem --- discover MSBuild/csc the documented way -------------------------
rem 1. pre-set CSC env var wins (explicit user override, kept as-is)
rem 2. vswhere -latest -find MSBuild\**\Bin\MSBuild.exe  (official probe)
rem 3. well-known drive roots as last resort
set "MSB="
if not defined CSC for /f "usebackq tokens=* delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do set "MSB=%%i"
if not defined CSC if not defined MSB for %%d in (D: C:) do if not defined MSB if exist "%%d\Program Files\Program\MSBuild\Current\Bin\MSBuild.exe" set "MSB=%%d\Program Files\Program\MSBuild\Current\Bin\MSBuild.exe"

if defined MSB (
    for %%i in ("%MSB%") do set "MSBDIR=%%~dpi"
    if not defined CSC if exist "%MSBDIR%Roslyn\csc.exe" set "CSC=%MSBDIR%Roslyn\csc.exe"
)

if exist "%MSB%" (
    echo [studio] building via MSBuild + csproj...
    "%MSB%" studio\LittStudio.csproj -p:Configuration=Release -nologo -v:m
    if not errorlevel 1 (
        echo [studio] studio\LittStudio.exe OK - run it or: litt studio
        exit /b 0
    )
    echo [studio] MSBuild failed - falling back to raw csc
)

if not defined CSC (
    echo Roslyn csc.exe not found - install Visual Studio, or set CSC to its full path
    exit /b 1
)
echo [studio] building via csc -langversion:latest -optimize+ ...
"%CSC%" -nologo -target:winexe -optimize+ -langversion:latest ^
  -out:studio\LittStudio.exe ^
  studio\cs\LittStudio.cs
if errorlevel 1 (
    echo [studio] BUILD FAILED
    exit /b 1
)
echo [studio] studio\LittStudio.exe OK - run it or: litt studio
