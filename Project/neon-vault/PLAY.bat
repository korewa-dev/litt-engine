@echo off
rem neon-vault player
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8247
timeout /t 2 >nul
start http://127.0.0.1:8247/viewer/play.html