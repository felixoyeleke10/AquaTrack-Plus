@echo off
setlocal
cd /d "%~dp0"

set "PYTHON_CMD=py -3.13"

if not exist ".venv\Scripts\python.exe" (
    echo [AquaTrack] Creating Python virtual environment...
    %PYTHON_CMD% -m venv .venv
    if errorlevel 1 goto :error
)

if not exist ".venv\.aquatrack-installed" (
    echo [AquaTrack] Installing dependencies. This may take several minutes...
    ".venv\Scripts\python.exe" -m pip install --upgrade pip
    if errorlevel 1 goto :error
    ".venv\Scripts\python.exe" -m pip install -e ".[dev,dashboard]"
    if errorlevel 1 goto :error
    type nul > ".venv\.aquatrack-installed"
)

echo [AquaTrack] Starting dashboard at http://localhost:8501
".venv\Scripts\python.exe" -m streamlit run dashboard.py
if errorlevel 1 goto :error
exit /b 0

:error
echo.
echo [AquaTrack] Startup failed. Review the error above.
pause
exit /b 1

