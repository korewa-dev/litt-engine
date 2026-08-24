@echo off
rem crimson-fall player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8295
timeout /t 2 >nul
start http://127.0.0.1:8295/viewer/play.html