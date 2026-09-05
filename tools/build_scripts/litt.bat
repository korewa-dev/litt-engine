@echo off
rem litt - the one command for the whole engine (delegates to tools/litt.py)
setlocal
set "HERE=%~dp0"
python "%HERE%tools\litt.py" %*
