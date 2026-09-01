from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np

VIDEO_EXTENSIONS = {".mp4", ".avi", ".mov", ".mkv", ".mts"}


@dataclass(frozen=True, slots=True)
class VideoInfo:
    path: Path
    frame_count: int
    fps: float
    duration_seconds: float
    width: int
    height: int
    fourcc: str
    file_size_bytes: int


def _fourcc_to_str(fourcc_code: float) -> str:
    code = int(fourcc_code)
    chars = "".join(chr((code >> (8 * i)) & 0xFF) for i in range(4))
    return chars.strip() or "????"


def read_video_info(path: str | Path) -> VideoInfo:
    path = Path(path)
    if not path.is_file():
        raise ValueError(f"Not a file: {path}")
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise ValueError(f"OpenCV could not open video: {path}")
    try:
        frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
        fps = capture.get(cv2.CAP_PROP_FPS)
        width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fourcc = _fourcc_to_str(capture.get(cv2.CAP_PROP_FOURCC))
    finally:
        capture.release()
    duration = frame_count / fps if fps > 0 else 0.0
    return VideoInfo(
        path=path,
        frame_count=frame_count,
        fps=fps,
        duration_seconds=duration,
        width=width,
        height=height,
        fourcc=fourcc,
        file_size_bytes=path.stat().st_size,
    )


def sample_frames(
    path: str | Path, count: int = 6, start_fraction: float = 0.0, end_fraction: float = 1.0,
) -> list[tuple[int, np.ndarray]]:
    """Uniformly samples `count` frames (as BGR ndarrays) between
    `start_fraction`/`end_fraction` of the video's length. Seeks by frame
    index (CAP_PROP_POS_FRAMES) rather than reading sequentially — much
    faster for a handful of samples from a long clip, at the cost of exact
    seek accuracy on some codecs, which is an acceptable tradeoff for a
    preview/exploration pass rather than frame-exact extraction."""
    if not 0.0 <= start_fraction < end_fraction <= 1.0:
        raise ValueError("Require 0 <= start_fraction < end_fraction <= 1")
    info = read_video_info(path)
    if info.frame_count <= 0:
        raise ValueError(f"Video reports no frames: {path}")

    first = int(info.frame_count * start_fraction)
    last = max(first, int(info.frame_count * end_fraction) - 1)
    indices = sorted(set(np.linspace(first, last, num=count, dtype=int).tolist()))

    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise ValueError(f"OpenCV could not open video: {path}")
    frames: list[tuple[int, np.ndarray]] = []
    try:
        for index in indices:
            capture.set(cv2.CAP_PROP_POS_FRAMES, index)
            ok, frame = capture.read()
            if ok and frame is not None:
                frames.append((index, frame))
    finally:
        capture.release()
    return frames


def frame_quality_metrics(frame: np.ndarray) -> dict[str, float]:
    """Cheap, underwater-relevant signals ahead of 03_underwater_preprocessing:
    brightness and sharpness (is this frame even usable), plus per-channel
    means (underwater footage typically loses red first — a large blue/green
    vs. red gap here is exactly what color-correction will need to fix)."""
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blue_mean, green_mean, red_mean = (float(frame[:, :, i].mean()) for i in range(3))
    return {
        "brightness": float(gray.mean()),
        "sharpness": float(cv2.Laplacian(gray, cv2.CV_64F).var()),
        "blue_mean": blue_mean,
        "green_mean": green_mean,
        "red_mean": red_mean,
        "red_deficiency": (blue_mean + green_mean) / 2.0 - red_mean,
    }


def scan_video_directory(root: str | Path) -> list[VideoInfo]:
    root = Path(root)
    if not root.is_dir():
        raise ValueError(f"Not a directory: {root}")
    paths = sorted(p for p in root.rglob("*") if p.suffix.lower() in VIDEO_EXTENSIONS)
    return [read_video_info(p) for p in paths]
