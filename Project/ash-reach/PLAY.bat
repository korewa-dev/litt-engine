@echo off
rem ash-reach player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8211
timeout /t 2 >nul
start http://127.0.0.1:8211/viewer/play.html