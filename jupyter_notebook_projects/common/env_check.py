"""Environment/dependency checks used by 00_environment_check.ipynb.

Every check function returns a small dict so the notebook can collect
results into one table instead of scattering print() calls everywhere.
Each function is defensive: a missing package or missing hardware is a
normal, expected result here, not an error worth crashing the notebook.
"""

from __future__ import annotations

import importlib
import platform
import shutil
import subprocess
import sys
from typing import Any


def check_python() -> dict[str, Any]:
    return {
        "name": "Python",
        "found": True,
        "version": platform.python_version(),
        "detail": f"{platform.system()} {platform.release()} ({platform.machine()})",
    }


def check_import(module_name: str, friendly_name: str | None = None,
                  version_attr: str = "__version__") -> dict[str, Any]:
    """Try importing `module_name` and report whether it's available.

    Used for the straightforward "is this package installed" checks:
    OpenCV, Ultralytics, FiftyOne, pandas, NumPy, matplotlib, etc.
    """
    friendly_name = friendly_name or module_name
    try:
        module = importlib.import_module(module_name)
        version = getattr(module, version_attr, "unknown")
        return {"name": friendly_name, "found": True, "version": str(version), "detail": ""}
    except Exception as exc:  # noqa: BLE001 - we want to report *any* import failure
        return {"name": friendly_name, "found": False, "version": None, "detail": str(exc)}


def check_pytorch_cuda() -> dict[str, Any]:
    """Report PyTorch version plus CUDA/cuDNN/GPU availability through PyTorch."""
    try:
        import torch
    except Exception as exc:  # noqa: BLE001
        return {"name": "PyTorch", "found": False, "version": None, "detail": str(exc)}

    cuda_available = torch.cuda.is_available()
    if cuda_available:
        device_count = torch.cuda.device_count()
        device_names = [torch.cuda.get_device_name(i) for i in range(device_count)]
        cudnn_version = torch.backends.cudnn.version()
        detail = (
            f"CUDA {torch.version.cuda} | cuDNN {cudnn_version} | "
            f"{device_count} GPU(s): {', '.join(device_names)}"
        )
    else:
        detail = "CUDA not available to PyTorch (CPU only, or no compatible GPU/driver)"

    return {
        "name": "PyTorch",
        "found": True,
        "version": torch.__version__,
        "detail": detail,
        "cuda_available": cuda_available,
    }


def check_nvidia_smi() -> dict[str, Any]:
    """Report raw `nvidia-smi` output, independent of what PyTorch sees.

    This is useful when PyTorch reports no CUDA device but the driver is
    actually fine (e.g. a CPU-only PyTorch build was installed by mistake).
    """
    exe = shutil.which("nvidia-smi")
    if exe is None:
        return {"name": "NVIDIA GPU (nvidia-smi)", "found": False, "version": None,
                "detail": "nvidia-smi not found on PATH"}
    try:
        out = subprocess.run(
            [exe, "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"],
            capture_output=True, text=True, timeout=10, check=True,
        )
        lines = [line.strip() for line in out.stdout.strip().splitlines() if line.strip()]
        return {"name": "NVIDIA GPU (nvidia-smi)", "found": True, "version": None,
                "detail": "; ".join(lines) if lines else "no GPUs reported"}
    except Exception as exc:  # noqa: BLE001
        return {"name": "NVIDIA GPU (nvidia-smi)", "found": False, "version": None, "detail": str(exc)}


def check_ffmpeg() -> dict[str, Any]:
    exe = shutil.which("ffmpeg")
    if exe is None:
        return {"name": "FFmpeg", "found": False, "version": None, "detail": "ffmpeg not found on PATH"}
    try:
        out = subprocess.run([exe, "-version"], capture_output=True, text=True, timeout=10, check=True)
        first_line = out.stdout.splitlines()[0] if out.stdout else "unknown version"
        return {"name": "FFmpeg", "found": True, "version": first_line.replace("ffmpeg version ", ""),
                "detail": exe}
    except Exception as exc:  # noqa: BLE001
        return {"name": "FFmpeg", "found": False, "version": None, "detail": str(exc)}


def run_all_checks() -> list[dict[str, Any]]:
    """Run the full AquaTrack+ environment check suite and return one row per check."""
    results = [check_python(), check_pytorch_cuda(), check_nvidia_smi()]
    results.append(check_import("cv2", "OpenCV"))
    results.append(check_import("ultralytics", "Ultralytics YOLO"))
    results.append(check_import("fiftyone", "FiftyOne"))
    results.append(check_import("pandas", "pandas"))
    results.append(check_import("numpy", "NumPy"))
    results.append(check_import("matplotlib", "matplotlib"))
    results.append(check_ffmpeg())
    return results


def print_summary(results: list[dict[str, Any]]) -> None:
    """Print a clean, aligned AquaTrack+ environment summary table."""
    name_width = max(len(r["name"]) for r in results) + 2
    version_width = max((len(r["version"] or "") for r in results), default=7) + 2
    version_width = max(version_width, len("version") + 2)

    print("=" * 78)
    print("AquaTrack+ Environment Summary".center(78))
    print("=" * 78)
    header = f"{'Component':<{name_width}}{'Status':<10}{'Version':<{version_width}}Detail"
    print(header)
    print("-" * 78)
    for r in results:
        status = "OK" if r["found"] else "MISSING"
        version = r["version"] or "-"
        detail = r["detail"] or ""
        print(f"{r['name']:<{name_width}}{status:<10}{version:<{version_width}}{detail}")
    print("-" * 78)

    missing = [r["name"] for r in results if not r["found"]]
    if missing:
        print(f"Missing/unavailable: {', '.join(missing)}")
    else:
        print("All checked components are available.")
    print("=" * 78)
