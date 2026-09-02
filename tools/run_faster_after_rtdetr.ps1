param(
    [Parameter(Mandatory = $true)]
    [int]$RtDetrPid
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$logDirectory = Join-Path $repositoryRoot "runs\fish_detector_benchmark\logs"
$null = New-Item -ItemType Directory -Path $logDirectory -Force
$supervisorLog = Join-Path $logDirectory "overnight_supervisor.log"
$fasterLog = Join-Path $logDirectory "faster_rcnn_converged.log"

"$(Get-Date -Format o) waiting for RT-DETR PID $RtDetrPid" | Out-File $supervisorLog -Append
Wait-Process -Id $RtDetrPid -ErrorAction SilentlyContinue
"$(Get-Date -Format o) RT-DETR exited; starting convergence-controlled Faster R-CNN" | Out-File $supervisorLog -Append

$python = Join-Path $repositoryRoot ".venv\Scripts\python.exe"
$runner = Join-Path $repositoryRoot "tools\train_faster_rcnn.py"
& $python -u $runner *>> $fasterLog
$exitCode = $LASTEXITCODE
"$(Get-Date -Format o) Faster R-CNN exited with code $exitCode" | Out-File $supervisorLog -Append
exit $exitCode
