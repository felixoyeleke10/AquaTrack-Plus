@echo off
setlocal
cd /d "%~dp0"
set "BUILD_DIR=build_qc"
set "BUILD_TYPE=Debug"
call "%~dp0build_cpp.bat"
if errorlevel 1 exit /b 1
ctest --test-dir "%BUILD_DIR%" --output-on-failure
exit /b %errorlevel%
