@echo off
rem kingsfall-hollow - native desktop game window (no browser)
cd /d "%~dp0"
python play_native.py
if errorlevel 1 pause
