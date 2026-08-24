@echo off
rem kingsfall-hollow in the Litt Studio window
setlocal
set "ROOT=%~dp0..\.."
if defined LITT_ENGINE set "EXE=%LITT_ENGINE%"
if not defined LITT_ENGINE (
  if exist "%ROOT%\target\x86_64-pc-windows-gnu\release\litt.exe" set "EXE=%ROOT%\target\x86_64-pc-windows-gnu\release\litt.exe"
)
if not defined EXE if exist "%ROOT%\target\x86_64-pc-windows-gnu\debug\litt.exe" set "EXE=%ROOT%\target\x86_64-pc-windows-gnu\debug\litt.exe"
if not defined EXE (
  echo Litt engine exe not found - build it first ^(cargo build^) or set LITT_ENGINE
  pause
  exit /b 1
)
"%EXE%" studio "%~dp0"
