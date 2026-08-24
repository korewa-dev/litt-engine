@echo off
rem grim-march native window
cd /d "%~dp0"
python play_native.py
if errorlevel 1 pause