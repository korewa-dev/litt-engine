@echo off
setlocal
cd /d "%~dp0"

set "MAKE_CMD="
where mingw32-make >nul 2>nul && set "MAKE_CMD=mingw32-make"
if not defined MAKE_CMD (
  where make >nul 2>nul && set "MAKE_CMD=make"
)
if not defined MAKE_CMD (
  echo [build] GNU make or mingw32-make is required.
  exit /b 1
)

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=all"
if /I "%TARGET%"=="dither3d" set "TARGET=dither3d_demo"
if /I "%TARGET%"=="dither" set "TARGET=dither3d_demo"
if /I "%TARGET%"=="cli" set "TARGET=bin/littcli"

echo [build] %MAKE_CMD% %TARGET%
%MAKE_CMD% %TARGET%
exit /b %ERRORLEVEL%
