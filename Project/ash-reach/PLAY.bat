@echo off
rem ash-reach player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8467
timeout /t 2 >nul
start http://127.0.0.1:8467/viewer/play.html