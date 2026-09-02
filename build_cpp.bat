@echo off
setlocal
cd /d "%~dp0"

set "QT_DIR=C:\Qt\6.7.2\msvc2019_64"
if not defined BUILD_DIR set "BUILD_DIR=build"
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%V\VC\Auxiliary\Build\vcvars64.bat"
for %%V in (
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
    "C:\Program Files\Microsoft Visual Studio\18\Community"
    "C:\Program Files\Microsoft Visual Studio\18\Professional"
    "C:\Program Files\Microsoft Visual Studio\2022\Community"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional"
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
) do if not defined VCVARS if exist "%%~V\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%~V\VC\Auxiliary\Build\vcvars64.bat"

if not defined VCVARS (
    echo Visual Studio C++ tools were not found.
    exit /b 1
)
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo Qt was not found at %QT_DIR%.
    exit /b 1
)

call "%VCVARS%" >nul 2>&1
cmake -S . -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --parallel 2
if errorlevel 1 exit /b 1
"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\AquaTrack.exe" >nul
echo Built %BUILD_DIR%\AquaTrack.exe
