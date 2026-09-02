@echo off
setlocal
cd /d "%~dp0"
if not exist "build\AquaTrack.exe" call "%~dp0build_cpp.bat"
if errorlevel 1 pause & exit /b 1
start "" "build\AquaTrack.exe"
