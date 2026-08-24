@echo off
rem neon-vault native window
cd /d "%~dp0"
python play_native.py
if errorlevel 1 pause