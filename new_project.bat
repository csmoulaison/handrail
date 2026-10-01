:<<":"
@echo off
REM Sets up a new handrail project in the folder that contains this clone.
REM Runs in cmd on Windows and in sh on Linux:
REM   handrail\new_project.bat
REM   sh handrail/new_project.bat
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\new_project.ps1"
goto :end
:
sh "$(dirname "$0")/scripts/new_project.sh"
exit $?
:end
