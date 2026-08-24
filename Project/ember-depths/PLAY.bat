@echo off
rem ember-depths player - double-click launcher (uses port 8091)
cd /d "%~dp0"
start "litt-ember-server" /min python tools\serve_live.py --port 8091
timeout /t 2 >nul
start http://127.0.0.1:8091/viewer/play.html
