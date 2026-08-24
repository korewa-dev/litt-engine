@echo off
rem skyline-run player - double-click launcher (uses port 8092)
cd /d "%~dp0"
start "litt-skyline-server" /min python tools\serve_live.py --port 8092
timeout /t 2 >nul
start http://127.0.0.1:8092/viewer/play.html
