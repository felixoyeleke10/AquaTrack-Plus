$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$venvPython = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
$installMarker = Join-Path $PSScriptRoot ".venv\.aquatrack-installed"

try {
    if (-not (Test-Path -LiteralPath $venvPython)) {
        Write-Host "[AquaTrack] Creating Python virtual environment..." -ForegroundColor Cyan
        & py -3.13 -m venv .venv
        if ($LASTEXITCODE -ne 0) { throw "Virtual environment creation failed." }
    }

    if (-not (Test-Path -LiteralPath $installMarker)) {
        Write-Host "[AquaTrack] Installing dependencies. This may take several minutes..." -ForegroundColor Cyan
        & $venvPython -m pip install --upgrade pip
        if ($LASTEXITCODE -ne 0) { throw "pip upgrade failed." }
        & $venvPython -m pip install -e ".[dev,dashboard]"
        if ($LASTEXITCODE -ne 0) { throw "Dependency installation failed." }
        New-Item -ItemType File -Path $installMarker -Force | Out-Null
    }

    Write-Host "[AquaTrack] Starting dashboard at http://localhost:8501" -ForegroundColor Green
    & $venvPython -m streamlit run dashboard.py
    if ($LASTEXITCODE -ne 0) { throw "Streamlit exited with an error." }
}
catch {
    Write-Host "[AquaTrack] Startup failed: $($_.Exception.Message)" -ForegroundColor Red
    Read-Host "Press Enter to close"
    exit 1
}

