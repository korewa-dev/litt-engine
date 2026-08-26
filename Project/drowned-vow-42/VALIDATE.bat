@echo off
rem drowned-vow-42 headless validation (CI smoke)
cd /d "%~dp0"
python play_native.py --frames 60 --dummy
if errorlevel 1 pause