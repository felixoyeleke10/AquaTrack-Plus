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

rem Windows locks a running exe, so linking over it fails with LNK1168 (an
rem open window, or a leftover instance with no window). A running exe can
rem still be renamed, so move it aside and build a fresh one; the old copy
rem is deleted on a later build once nothing uses it.
del /q "%BUILD_DIR%\AquaTrack.in-use-*.exe" >nul 2>&1
set "AQUATRACK_EXE=%CD%\%BUILD_DIR%\AquaTrack.exe"
powershell -NoProfile -Command "$exe=$env:AQUATRACK_EXE; if (Get-Process AquaTrack -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) { exit 1 }"
if errorlevel 1 (
    ren "%BUILD_DIR%\AquaTrack.exe" "AquaTrack.in-use-%RANDOM%.exe"
    if errorlevel 1 (
        echo AquaTrack is running from %BUILD_DIR%\AquaTrack.exe and could not be moved aside.
        echo Close it ^(Task Manager, AquaTrack.exe, End task^) and run this again.
        exit /b 1
    )
    echo Note: an older AquaTrack is still running; building a new copy alongside it.
)

call "%VCVARS%" >nul 2>&1
rem The app is built optimised (Release): a Debug build with Qt's debug
rem libraries is roughly ten times slower, which shows up as seconds-long
rem pauses opening a project. test_cpp.bat sets BUILD_TYPE=Debug for tests.
if not defined BUILD_TYPE set "BUILD_TYPE=Release"
cmake -S . -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --parallel 2
if errorlevel 1 exit /b 1
"%QT_DIR%\bin\windeployqt.exe" "%BUILD_DIR%\AquaTrack.exe" >nul
echo Built %BUILD_DIR%\AquaTrack.exe
