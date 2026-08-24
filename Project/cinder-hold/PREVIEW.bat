@echo off
rem cinder-hold dev preview (NOT the game renderer)
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8382
timeout /t 2 >nul
start http://127.0.0.1:8382/viewer/