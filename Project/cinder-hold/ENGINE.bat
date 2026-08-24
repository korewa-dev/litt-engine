@echo off
rem Litt native launcher - engine plays this world in its own window
setlocal
set "HERE=%~dp0"
set "ROOT=%HERE%..\..\"
if defined LITT_ENGINE set "EXE=%LITT_ENGINE%"
if not defined EXE set "EXE=%ROOT%target\x86_64-pc-windows-gnu\release\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\x86_64-pc-windows-gnu\debug\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\release\litt.exe"
if not exist "%EXE%" set "EXE=%ROOT%target\debug\litt.exe"
"%EXE%" play "%HERE:~0,-1%"
