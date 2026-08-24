@echo off
rem build-studio.bat - compile the C# Litt Studio with the VS-bundled
rem Roslyn compiler (no dotnet SDK needed) against the installed desktop
rem runtime. Output: studio\LittStudio.exe
setlocal enabledelayedexpansion
cd /d "%~dp0.."

set "CSC=D:\Program Files\Program\MSBuild\Current\Bin\Roslyn\csc.exe"
if not exist "%CSC%" set "CSC=C:\Program Files\Program\MSBuild\Current\Bin\Roslyn\csc.exe"
if not exist "%CSC%" (
    echo Roslyn csc.exe not found - install Visual Studio or set CSC
    exit /b 1
)

set "CORE=%ProgramFiles%\dotnet\shared\Microsoft.NETCore.App"
for /f "delims=" %%d in ('dir /b /ad "%CORE%" ^| sort') do set "CV=%%d"
set "CORE=%CORE%\%CV%"

set "WDT=%ProgramFiles%\dotnet\shared\Microsoft.WindowsDesktop.App"
for /f "delims=" %%d in ('dir /b /ad "%WDT%" ^| sort') do set "WV=%%d"
set "WDT=%WDT%\%WV%"

if not exist "%CORE%\System.Private.CoreLib.dll" (
    echo .NET runtime not found under ProgramFiles\dotnet\shared
    exit /b 1
)

if not exist studio mkdir studio
"%CSC%" -nologo -target:winexe -optimize+ -langversion:latest ^
  -out:studio\LittStudio.exe ^
  studio\cs\LittStudio.cs

if errorlevel 1 (
    echo [studio] BUILD FAILED
    exit /b 1
)
echo [studio] studio\LittStudio.exe OK - run it or: litt studio
