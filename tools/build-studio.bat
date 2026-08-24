@echo off
rem build-studio.bat - compile the C# Litt Studio.
rem Primary path : VS-bundled MSBuild + LittStudio.csproj (documented
rem                Microsoft.Common.CurrentVersion.targets machinery,
rem                FrameworkPathOverride covers missing ref assemblies).
rem Fallback path: raw Roslyn csc against the machine's framework dirs.
setlocal
cd /d "%~dp0.."

set "MSB=D:\Program Files\Program\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSB%" set "MSB=C:\Program Files\Program\MSBuild\Current\Bin\MSBuild.exe"

if exist "%MSB%" (
    echo [studio] building via MSBuild + csproj...
    "%MSB%" studio\LittStudio.csproj -p:Configuration=Release -nologo -v:m
    if not errorlevel 1 (
        echo [studio] studio\LittStudio.exe OK - run it or: litt studio
        exit /b 0
    )
    echo [studio] MSBuild failed - falling back to raw csc
)

set "CSC=D:\Program Files\Program\MSBuild\Current\Bin\Roslyn\csc.exe"
if not exist "%CSC%" set "CSC=C:\Program Files\Program\MSBuild\Current\Bin\Roslyn\csc.exe"
if not exist "%CSC%" (
    echo Roslyn csc.exe not found - install Visual Studio or set CSC
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
