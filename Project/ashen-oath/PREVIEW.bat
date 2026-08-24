@echo off
rem ashen-oath dev preview (NOT the game renderer)
cd /d "%~dp0"
start "litt-server" /min python tools\serve_live.py --port 8177
timeout /t 2 >nul
start http://127.0.0.1:8177/viewer/