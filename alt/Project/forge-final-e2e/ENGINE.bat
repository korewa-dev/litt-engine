@echo off
rem Litt native launcher - engine plays this world in its own window
setlocal
set "HERE=%~dp0"
set "HERE=%HERE:~0,-1%"
set "ROOT=%HERE%\..\..\"
if defined LITT_ENGINE set "EXE=%LITT_ENGINE%"
if not defined EXE set "EXE=%ROOT%\native\bin\littview.exe"
if not exist "%EXE%" (
    echo [engine] littview.exe not found - run native/build.bat
    goto :eof
)
"%EXE%" window "%HERE%"