from __future__ import annotations

from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"}
VIDEO_EXTENSIONS = {".mp4", ".avi", ".mov", ".mkv", ".mts"}
ANNOTATION_EXTENSIONS = {".txt", ".json", ".xml", ".csv"}

# Known dataset kinds this notebook layer is aware of. Only "brackishmot" has a
# specialized adapter today (aquatrack.datasets.brackishmot) — the rest scan
# generically until a matching <name>.py module is added following that file's
# pattern. Keep this list in sync with what 01_dataset_exploration.ipynb offers.
KNOWN_DATASET_KINDS = (
    "brackishmot",
    "sfishtrack",
    "mft25",
    "fathomnet",
    "noaa",
    "onc",  # Ocean Networks Canada — not yet public/available locally
    "generic",
)


@dataclass(slots=True)
class DirectoryScan:
    root: Path
    total_files: int
    total_dirs: int
    extension_counts: Counter
    image_count: int
    video_count: int
    annotation_count: int
    top_level_entries: list[str]
    sample_images: list[Path] = field(default_factory=list)
    sample_videos: list[Path] = field(default_factory=list)
    truncated: bool = False


def scan_directory(root: str | Path, max_files: int = 200_000, sample_size: int = 6) -> DirectoryScan:
    """A dataset-agnostic walk: file/extension tallies plus a few sample media
    paths, so any dataset (known adapter or not) gets an immediate, honest
    picture of what's actually on disk before anything dataset-specific runs."""
    root = Path(root)
    if not root.is_dir():
        raise ValueError(f"Not a directory: {root}")

    extension_counts: Counter = Counter()
    sample_images: list[Path] = []
    sample_videos: list[Path] = []
    total_files = 0
    total_dirs = 0
    truncated = False

    for path in root.rglob("*"):
        if path.is_dir():
            total_dirs += 1
            continue
        total_files += 1
        if total_files > max_files:
            truncated = True
            break
        suffix = path.suffix.lower()
        extension_counts[suffix] += 1
        if suffix in IMAGE_EXTENSIONS and len(sample_images) < sample_size:
            sample_images.append(path)
        elif suffix in VIDEO_EXTENSIONS and len(sample_videos) < sample_size:
            sample_videos.append(path)

    image_count = sum(count for ext, count in extension_counts.items() if ext in IMAGE_EXTENSIONS)
    video_count = sum(count for ext, count in extension_counts.items() if ext in VIDEO_EXTENSIONS)
    annotation_count = sum(
        count for ext, count in extension_counts.items() if ext in ANNOTATION_EXTENSIONS
    )
    top_level_entries = sorted(p.name for p in root.iterdir())

    return DirectoryScan(
        root=root,
        total_files=total_files,
        total_dirs=total_dirs,
        extension_counts=extension_counts,
        image_count=image_count,
        video_count=video_count,
        annotation_count=annotation_count,
        top_level_entries=top_level_entries,
        sample_images=sample_images,
        sample_videos=sample_videos,
        truncated=truncated,
    )


def looks_like_brackishmot(root: str | Path) -> bool:
    root = Path(root)
    return any((root / split).is_dir() for split in ("train", "test")) and any(
        (root / split).is_dir() and any((root / split).glob("*/seqinfo.ini"))
        for split in ("train", "test")
        if (root / split).is_dir()
    )


def explore_brackishmot(root: str | Path) -> dict:
    from aquatrack.datasets.brackishmot import discover_sequences, parse_annotations, read_sequence_info

    discovered = discover_sequences(root)
    report: dict = {"kind": "brackishmot", "root": str(Path(root).resolve()), "splits": {}}
    for split, sequences in discovered.items():
        split_report = []
        for sequence_path in sequences:
            info = read_sequence_info(sequence_path)
            annotations = parse_annotations(sequence_path)
            class_counts = Counter(a.class_id for a in annotations)
            track_ids = {a.track_id for a in annotations}
            split_report.append({
                "name": info.name,
                "frames": info.length,
                "frame_rate": info.frame_rate,
                "resolution": f"{info.width}x{info.height}",
                "annotations": len(annotations),
                "unique_tracks": len(track_ids),
                "class_counts": dict(class_counts),
            })
        report["splits"][split] = split_report
    return report


def looks_like_sfishtrack(root: str | Path) -> bool:
    root = Path(root)
    return (root / "Annotations").is_dir() and any(
        (root / "Annotations").glob("video*.json")
    )


def explore_sfishtrack(root: str | Path) -> dict:
    from aquatrack.datasets.sfishtrack import discover_videos, read_video_annotations

    report: dict = {"kind": "sfishtrack", "root": str(Path(root).resolve()), "videos": []}
    for annotation_path in discover_videos(root):
        video = read_video_annotations(annotation_path)
        track_ids = {a.track_id for a in video.annotations}
        category_counts = Counter(
            video.categories.get(a.category_id, str(a.category_id)) for a in video.annotations
        )
        report["videos"].append({
            "name": video.name,
            "frames": len(video.images),
            "resolution": (
                f"{video.images[0].width}x{video.images[0].height}" if video.images else "unknown"
            ),
            "annotations": len(video.annotations),
            "unique_tracks": len(track_ids),
            "class_counts": dict(category_counts),
            "has_video_file": video.video_path is not None,
            "has_masks": video.masks_dir is not None,
        })
    return report


# name -> (looks_like predicate, explorer). Add a same-shaped entry here plus a
# new aquatrack/datasets/<name>.py module (see brackishmot.py / sfishtrack.py)
# to bring another dataset kind online.
_ADAPTERS: dict[str, tuple[Callable[[Path], bool], Callable[[Path], dict]]] = {
    "brackishmot": (looks_like_brackishmot, explore_brackishmot),
    "sfishtrack": (looks_like_sfishtrack, explore_sfishtrack),
}


def explore_dataset(root: str | Path, kind: str = "generic") -> dict:
    """Dispatches to a specialized adapter for `kind` when one exists, else
    returns a generic directory scan. `kind="generic"` always uses the scan,
    even if the directory happens to match a known layout — pass the specific
    kind to opt into specialized parsing."""
    root = Path(root)
    if kind not in KNOWN_DATASET_KINDS:
        raise ValueError(f"Unknown dataset kind {kind!r}; choose one of {KNOWN_DATASET_KINDS}")

    if kind in _ADAPTERS:
        return _ADAPTERS[kind][1](root)

    scan = scan_directory(root)
    return {
        "kind": kind,
        "root": str(root.resolve()),
        "note": (
            f"No specialized adapter for {kind!r} yet — showing a generic file scan. "
            f"Add aquatrack/datasets/{kind}.py following brackishmot.py's pattern, "
            "then register it in aquatrack/datasets/explore.py's _ADAPTERS."
        ),
        "total_files": scan.total_files,
        "total_dirs": scan.total_dirs,
        "image_count": scan.image_count,
        "video_count": scan.video_count,
        "annotation_count": scan.annotation_count,
        "top_level_entries": scan.top_level_entries,
        "extension_counts": dict(scan.extension_counts.most_common(15)),
        "sample_images": [str(p) for p in scan.sample_images],
        "sample_videos": [str(p) for p in scan.sample_videos],
        "truncated": scan.truncated,
    }
