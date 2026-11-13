@echo off
setlocal
cd /d "%~dp0"
rem Always run an incremental build first (seconds when nothing changed), so
rem the launcher never starts an out-of-date AquaTrack.exe.
call "%~dp0build_cpp.bat"
if errorlevel 1 pause & exit /b 1
start "" "build\AquaTrack.exe"
