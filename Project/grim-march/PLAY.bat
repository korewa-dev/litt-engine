@echo off
rem grim-march player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8445
timeout /t 2 >nul
start http://127.0.0.1:8445/viewer/play.html