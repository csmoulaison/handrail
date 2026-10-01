:<<":"
@echo off
REM Builds the game. Runs in cmd on Windows and in sh on Linux:
REM   build.bat [bootstrap|static|dynamic|all|clean] [debug|release]
REM   sh build.bat [bootstrap|static|dynamic|all|clean] [debug|release]
REM Defaults to all debug. Output goes to bin\, intermediates to build\.
set TARGET=%1
set MODE=%2
if "%TARGET%"=="" set TARGET=all
if "%MODE%"=="" set MODE=debug
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File handrail\scripts\build_windows.ps1 __NAME__ %TARGET% %MODE%
goto :end
:
cd "$(dirname "$0")" && sh handrail/scripts/build_linux.sh __NAME__ "${1:-all}" "${2:-debug}"
exit $?
:end
