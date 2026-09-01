"""Dataset exploration helpers used by 01_dataset_exploration.ipynb.

These helpers are dataset-agnostic on purpose: AquaTrack+ needs to work
against SFISHTRACK, BrackishMOT, MFT25, FathomNet, NOAA underwater fish
data, and eventually Ocean Networks Canada footage, and those datasets
don't share one folder layout or annotation format. So instead of one
loader per dataset, these functions just answer generic questions
("what's in this folder?", "does this look like COCO/YOLO/MOT?") that
work no matter which dataset DATASET_ROOT points at.
"""

from __future__ import annotations

import json
from collections import Counter
from pathlib import Path
from typing import Any

IMAGE_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
VIDEO_EXTS = {".mp4", ".avi", ".mov", ".mkv", ".mts"}
ANNOTATION_EXTS = {".json", ".txt", ".xml", ".csv"}


def print_directory_tree(root: Path, max_depth: int = 2, max_entries_per_dir: int = 15) -> None:
    """Print a shallow directory tree so a new dataset's layout is visible at a glance."""
    root = Path(root)
    if not root.exists():
        print(f"Path does not exist: {root}")
        return

    print(f"{root}/")

    def _walk(dir_path: Path, prefix: str, depth: int) -> None:
        if depth > max_depth:
            return
        try:
            entries = sorted(dir_path.iterdir(), key=lambda p: (p.is_file(), p.name.lower()))
        except PermissionError:
            print(f"{prefix}[permission denied]")
            return

        shown = entries[:max_entries_per_dir]
        for i, entry in enumerate(shown):
            is_last = i == len(shown) - 1
            connector = "\\-- " if is_last else "|-- "
            suffix = "/" if entry.is_dir() else ""
            print(f"{prefix}{connector}{entry.name}{suffix}")
            if entry.is_dir():
                extension = "    " if is_last else "|   "
                _walk(entry, prefix + extension, depth + 1)

        if len(entries) > max_entries_per_dir:
            print(f"{prefix}    ... ({len(entries) - max_entries_per_dir} more entries)")

    _walk(root, "", 1)


def count_files_by_extension(root: Path) -> Counter:
    """Recursively count files under `root` grouped by lowercase extension."""
    root = Path(root)
    counts: Counter = Counter()
    if not root.exists():
        return counts
    for path in root.rglob("*"):
        if path.is_file():
            counts[path.suffix.lower()] += 1
    return counts


def summarize_media(root: Path) -> dict[str, Any]:
    """Summarize how many images/videos/annotation-like files exist under `root`."""
    counts = count_files_by_extension(root)
    images = sum(n for ext, n in counts.items() if ext in IMAGE_EXTS)
    videos = sum(n for ext, n in counts.items() if ext in VIDEO_EXTS)
    annotations = sum(n for ext, n in counts.items() if ext in ANNOTATION_EXTS)
    return {
        "root": str(root),
        "image_files": images,
        "video_files": videos,
        "annotation_like_files": annotations,
        "extension_breakdown": dict(counts.most_common(20)),
    }


def guess_annotation_format(root: Path) -> dict[str, Any]:
    """Best-effort heuristic guess at which annotation format a dataset uses.

    Looks for tell-tale filenames/structures rather than trying to fully
    parse anything:
      - COCO-style:  one or more *.json files with "images"/"annotations" keys
      - MOT-style:   gt/gt.txt or *_gt.txt with comma-separated rows
      - YOLO-style:  labels/ directory full of per-image *.txt files
    """
    root = Path(root)
    findings: list[str] = []

    coco_candidates = list(root.rglob("*.json"))[:20]
    for candidate in coco_candidates:
        try:
            with open(candidate, "r", encoding="utf-8") as f:
                data = json.load(f)
            if isinstance(data, dict) and {"images", "annotations"} & set(data.keys()):
                findings.append(f"COCO-like JSON: {candidate}")
                break
        except Exception:
            continue

    mot_candidates = list(root.rglob("gt.txt")) + list(root.rglob("*_gt.txt"))
    if mot_candidates:
        findings.append(f"MOT-like gt.txt found: {mot_candidates[0]} (+{len(mot_candidates) - 1} more)"
                         if len(mot_candidates) > 1 else f"MOT-like gt.txt found: {mot_candidates[0]}")

    labels_dirs = [p for p in root.rglob("labels") if p.is_dir()]
    if labels_dirs:
        sample_txts = list(labels_dirs[0].glob("*.txt"))
        if sample_txts:
            findings.append(f"YOLO-like labels/ directory found: {labels_dirs[0]} "
                             f"({len(sample_txts)} .txt files)")

    return {
        "root": str(root),
        "likely_formats": findings if findings else ["unknown / not detected"],
    }


def find_sample_files(root: Path, extensions: set[str], limit: int = 5) -> list[Path]:
    """Return up to `limit` files under `root` matching one of `extensions`."""
    root = Path(root)
    if not root.exists():
        return []
    matches: list[Path] = []
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in extensions:
            matches.append(path)
            if len(matches) >= limit:
                break
    return matches
