from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True, slots=True)
class SfishtrackImage:
    id: int
    file_name: str
    height: int
    width: int
    frame_id: int
    video_id: int


@dataclass(frozen=True, slots=True)
class SfishtrackAnnotation:
    id: int
    image_id: int
    category_id: int
    bbox: tuple[float, float, float, float]
    area: float
    iscrowd: int
    track_id: int


@dataclass(frozen=True, slots=True)
class SfishtrackVideo:
    video_id: int
    name: str
    annotation_path: Path
    frames_dir: Path | None
    masks_dir: Path | None
    video_path: Path | None
    metadata_path: Path | None
    images: list[SfishtrackImage]
    annotations: list[SfishtrackAnnotation]
    categories: dict[int, str]


def _video_number(annotation_stem: str) -> str:
    # "video001" -> "001", matching the "video_001.mp4" / "video_001" / "masks_001"
    # sibling-folder naming (Annotations uses no underscore, everything else does).
    digits = "".join(ch for ch in annotation_stem if ch.isdigit())
    if not digits:
        raise ValueError(f"Could not extract a video number from {annotation_stem!r}")
    return digits


def read_video_annotations(annotation_path: str | Path) -> SfishtrackVideo:
    annotation_path = Path(annotation_path)
    with annotation_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)

    videos = data.get("videos") or []
    if not videos:
        raise ValueError(f"No 'videos' entry in {annotation_path}")

    categories = {c["id"]: c["name"] for c in data.get("categories", [])}
    images = [
        SfishtrackImage(
            id=img["id"], file_name=img["file_name"], height=img["height"],
            width=img["width"], frame_id=img["frame_id"], video_id=img["video_id"],
        )
        for img in data.get("images", [])
    ]
    annotations = [
        SfishtrackAnnotation(
            id=ann["id"], image_id=ann["image_id"], category_id=ann["category_id"],
            bbox=tuple(ann["bbox"]), area=ann["area"], iscrowd=ann["iscrowd"],
            track_id=ann["track_id"],
        )
        for ann in data.get("annotations", [])
    ]

    dataset_root = annotation_path.parent.parent  # .../Annotations/videoNNN.json -> dataset root
    number = _video_number(annotation_path.stem)
    frames_dir = dataset_root / "Frames" / f"video_{number}"
    masks_dir = dataset_root / "Masks" / f"masks_{number}"
    video_path = dataset_root / "Videos" / f"video_{number}.mp4"
    metadata_path = dataset_root / "metadata" / f"video_{number}.json"

    return SfishtrackVideo(
        video_id=videos[0]["id"],
        # Derived from the filename, not videos[0]["name"]: every annotation
        # file in this dataset carries the same literal "video_1" there, so
        # trusting it would make every video indistinguishable in reports.
        name=f"video_{number}",
        annotation_path=annotation_path,
        frames_dir=frames_dir if frames_dir.is_dir() else None,
        masks_dir=masks_dir if masks_dir.is_dir() else None,
        video_path=video_path if video_path.is_file() else None,
        metadata_path=metadata_path if metadata_path.is_file() else None,
        images=images,
        annotations=annotations,
        categories=categories,
    )


def discover_videos(dataset_root: str | Path) -> list[Path]:
    dataset_root = Path(dataset_root)
    annotations_dir = dataset_root / "Annotations"
    if not annotations_dir.is_dir():
        raise ValueError(f"Missing SFISHTRACK Annotations directory: {annotations_dir}")
    paths = sorted(annotations_dir.glob("video*.json"))
    if not paths:
        raise ValueError(f"No annotation files found in {annotations_dir}")
    return paths


def read_video_metadata(video: SfishtrackVideo) -> dict:
    """The free-text recording metadata (date, depth, camera, location) SFISHTRACK
    ships per video — genuinely useful for a marine survey tool, unlike BrackishMOT
    which has none of this, so it's kept separate rather than forced into a shared
    schema the other adapter would just leave empty."""
    if video.metadata_path is None:
        return {}
    with video.metadata_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)
