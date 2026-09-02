@echo off
setlocal
cd /d "%~dp0"
set "CLI_EXE=build\aquatrack-cli.exe"
if not exist "%CLI_EXE%" if exist "build_qc\aquatrack-cli.exe" set "CLI_EXE=build_qc\aquatrack-cli.exe"
if not exist "%CLI_EXE%" call "%~dp0build_cpp.bat"
if errorlevel 1 exit /b 1
"%CLI_EXE%" %*
exit /b %errorlevel%
