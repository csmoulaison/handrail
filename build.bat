:<<":":
@echo off
REM Windows Build
powershell -ExecutionPolicy Bypass -File handrail\scripts\build_windows.ps1 example all debug
goto :end
:
# Linux Build
sh handrail/scripts/build_linux.sh example all debug
:end