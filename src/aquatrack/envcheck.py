from __future__ import annotations

import importlib
import shutil
import subprocess
import sys
from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True, slots=True)
class CheckResult:
    name: str
    ok: bool
    detail: str
    required: bool = True

    @property
    def status(self) -> str:
        if self.ok:
            return "OK"
        return "MISSING" if self.required else "OPTIONAL"


def _try_import(module_name: str):
    try:
        return importlib.import_module(module_name)
    except ImportError:
        return None


def check_python() -> CheckResult:
    version = sys.version.split()[0]
    ok = sys.version_info >= (3, 11)
    return CheckResult(
        "Python", ok,
        f"{version} at {sys.executable}" + ("" if ok else " (need >= 3.11)"),
    )


def check_numpy() -> CheckResult:
    numpy = _try_import("numpy")
    if numpy is None:
        return CheckResult("NumPy", False, "not installed")
    return CheckResult("NumPy", True, numpy.__version__)


def check_pandas() -> CheckResult:
    pandas = _try_import("pandas")
    if pandas is None:
        return CheckResult("pandas", False, "not installed")
    return CheckResult("pandas", True, pandas.__version__)


def check_matplotlib() -> CheckResult:
    matplotlib = _try_import("matplotlib")
    if matplotlib is None:
        return CheckResult("matplotlib", False, "not installed")
    return CheckResult("matplotlib", True, matplotlib.__version__)


def check_opencv() -> CheckResult:
    cv2 = _try_import("cv2")
    if cv2 is None:
        return CheckResult("OpenCV", False, "not installed (opencv-python[-headless])")
    build_info = getattr(cv2, "getBuildInformation", lambda: "")()
    cuda_ok = "NVIDIA CUDA" in build_info and "YES" in build_info.split(
        "NVIDIA CUDA"
    )[1].splitlines()[0]
    detail = cv2.__version__ + (" (CUDA build)" if cuda_ok else "")
    return CheckResult("OpenCV", True, detail)


def check_pytorch() -> CheckResult:
    torch = _try_import("torch")
    if torch is None:
        return CheckResult("PyTorch", False, "not installed")
    return CheckResult("PyTorch", True, torch.__version__)


def check_cuda() -> CheckResult:
    """CUDA availability as seen by the installed PyTorch build specifically —
    a machine can have a working NVIDIA driver while torch itself is still the
    CPU-only wheel, which is a distinct (and very easy to make) failure mode."""
    torch = _try_import("torch")
    if torch is None:
        return CheckResult("CUDA (via PyTorch)", False, "PyTorch not installed", required=False)
    build = torch.version.cuda
    if build is None:
        return CheckResult(
            "CUDA (via PyTorch)", False,
            "torch is a CPU-only build (torch.version.cuda is None) — "
            "reinstall from https://pytorch.org/get-started/locally/ with a CUDA index",
            required=False,
        )
    available = torch.cuda.is_available()
    detail = f"torch built for CUDA {build}, cuda.is_available()={available}"
    return CheckResult("CUDA (via PyTorch)", available, detail, required=False)


def check_gpu() -> CheckResult:
    # Not required: CPU-only development (dataset exploration, small-scale
    # detection smoke tests) is a legitimate path, just slower. GPU/CUDA
    # status is still surfaced prominently because it matters for real
    # training/inference throughput, and later notebooks (TensorRT export)
    # do need it — but this check alone shouldn't block getting started.
    torch = _try_import("torch")
    if torch is not None and torch.cuda.is_available():
        names = [torch.cuda.get_device_name(i) for i in range(torch.cuda.device_count())]
        return CheckResult("NVIDIA GPU", True, ", ".join(names), required=False)
    # Fall back to nvidia-smi so this still reports a real GPU even when torch
    # can't see it (e.g. CPU-only torch build) — that mismatch is the point.
    smi = shutil.which("nvidia-smi")
    if smi is None:
        return CheckResult(
            "NVIDIA GPU", False, "no NVIDIA driver found (nvidia-smi missing)", required=False,
        )
    try:
        output = subprocess.run(
            [smi, "--query-gpu=name,driver_version,memory.total", "--format=csv,noheader"],
            capture_output=True, text=True, timeout=10, check=True,
        ).stdout.strip()
    except (subprocess.SubprocessError, OSError) as exc:
        return CheckResult("NVIDIA GPU", False, f"nvidia-smi failed: {exc}", required=False)
    return CheckResult(
        "NVIDIA GPU", True,
        f"{output} (driver present, but not visible to the current PyTorch build)"
        if torch is not None else output,
        required=False,
    )


def check_cudnn() -> CheckResult:
    torch = _try_import("torch")
    if torch is None or not torch.backends.cudnn.is_available():
        return CheckResult("cuDNN", False, "not available", required=False)
    version = torch.backends.cudnn.version()
    return CheckResult("cuDNN", True, f"version {version}")


def check_ultralytics() -> CheckResult:
    ultralytics = _try_import("ultralytics")
    if ultralytics is None:
        return CheckResult("Ultralytics YOLO", False, "not installed")
    return CheckResult("Ultralytics YOLO", True, ultralytics.__version__)


def check_fiftyone() -> CheckResult:
    fiftyone = _try_import("fiftyone")
    if fiftyone is None:
        return CheckResult("FiftyOne", False, "not installed", required=False)
    return CheckResult("FiftyOne", True, fiftyone.__version__, required=False)


def check_ffmpeg() -> CheckResult:
    path = shutil.which("ffmpeg")
    if path is None:
        return CheckResult("FFmpeg", False, "not found on PATH")
    try:
        first_line = subprocess.run(
            [path, "-version"], capture_output=True, text=True, timeout=10, check=True,
        ).stdout.splitlines()[0]
    except (subprocess.SubprocessError, OSError, IndexError) as exc:
        return CheckResult("FFmpeg", False, f"found at {path} but failed to run: {exc}")
    return CheckResult("FFmpeg", True, first_line)


# Order matters for display: identity/runtime first, then the GPU story as one
# connected narrative (driver -> torch's CUDA build -> cuDNN), then libraries.
_CHECKS: list[Callable[[], CheckResult]] = [
    check_python,
    check_numpy,
    check_pandas,
    check_matplotlib,
    check_opencv,
    check_pytorch,
    check_gpu,
    check_cuda,
    check_cudnn,
    check_ultralytics,
    check_fiftyone,
    check_ffmpeg,
]


def run_all_checks() -> list[CheckResult]:
    return [check() for check in _CHECKS]


def summary_table(results: list[CheckResult]):
    """Returns a pandas DataFrame when pandas is available (nice notebook
    display), otherwise a plain list of row tuples so this still works even
    when the environment is missing the very thing it's reporting on."""
    rows = [(r.name, r.status, r.detail) for r in results]
    pandas = _try_import("pandas")
    if pandas is None:
        return rows
    return pandas.DataFrame(rows, columns=["Component", "Status", "Detail"])


def print_summary(results: list[CheckResult]) -> None:
    missing_required = [r for r in results if r.required and not r.ok]
    name_width = max(len(r.name) for r in results)
    status_width = max(len(r.status) for r in results)
    print("AquaTrack+ environment summary")
    print("=" * 60)
    for r in results:
        marker = "[OK]  " if r.ok else ("[!!]  " if r.required else "[--]  ")
        print(f"{marker}{r.name:<{name_width}}  {r.status:<{status_width}}  {r.detail}")
    print("=" * 60)
    if missing_required:
        names = ", ".join(r.name for r in missing_required)
        print(f"{len(missing_required)} required component(s) missing: {names}")
    else:
        print("All required components present.")
