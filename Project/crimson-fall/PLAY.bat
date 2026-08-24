@echo off
rem crimson-fall player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8342
timeout /t 2 >nul
start http://127.0.0.1:8342/viewer/play.html