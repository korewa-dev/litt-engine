@echo off
rem example-village player - double-click launcher (uses port 8089)
cd /d "%~dp0"
start "litt-village-server" /min python tools\serve_live.py --port 8089
timeout /t 2 >nul
start http://127.0.0.1:8089/viewer/play.html