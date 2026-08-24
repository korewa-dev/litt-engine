@echo off
rem kingsfall-hollow player - double-click launcher (uses port 8093)
cd /d "%~dp0"
start "litt-kingsfall-server" /min python tools\serve_live.py --port 8093
timeout /t 2 >nul
start http://127.0.0.1:8093/viewer/play.html
