@echo off
rem crimson-fall native window
cd /d "%~dp0"
python play_native.py
if errorlevel 1 pause