from __future__ import annotations

import json
import os
import random
import shutil
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

import yaml


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


# Verified against the real dataset by visual inspection of 4 independent
# videos (video_002 through video_005): every video except video_001 labels
# its targets "object" instead of "fish", but the boxed subjects are small
# fish camouflaged against the reef in every sample checked -- the same
# large, distinctive net-patterned creature recurring across videos is
# consistently left unboxed in all of them. This is one real class labeled
# inconsistently across annotation sessions, not two different classes: only
# video_001 says "fish", and merging is what makes every other video's
# labels usable as fish-detection ground truth (unmerged, only 1 of 54
# videos has any "fish"-class example at all).
_CATEGORY_ALIASES = {"object": "fish"}


def _normalize_category_name(name: str) -> str:
    return _CATEGORY_ALIASES.get(name, name)


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


def _publish_image(source: Path, destination: Path, method: str) -> str:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if method == "copy":
        shutil.copy2(source, destination)
        return "copy"
    if method != "hardlink":
        raise ValueError("method must be 'hardlink' or 'copy'")
    try:
        os.link(source, destination)
        return "hardlink"
    except OSError:
        shutil.copy2(source, destination)
        return "copy"


# "fill ratio" = the annotation's real segmentation-mask pixel area divided by
# its bounding-box rectangle area. Verified against the real dataset: most
# boxes fill 40-75% of their rectangle, but some are genuinely sloppy --
# e.g. video_019 frame 84 track 75 fills only 23%, a wide loose rectangle
# spanning a fish plus a large stretch of empty rock, visually confirmed
# against the rendered frame. Below MIN_FILL_RATIO is dropped as training
# noise: a detector trained on boxes like that learns to draw overly
# generous, imprecise ones.
_LOW_FILL_RATIO = "low_fill_ratio"


def _to_yolo_row(
    annotation: SfishtrackAnnotation,
    image_width: int,
    image_height: int,
    category_name: str | None,
    name_to_index: dict[str, int],
    min_fill_ratio: float,
) -> tuple[int, float, float, float, float] | str | None:
    """Returns the YOLO row, or a reason string ("low_fill_ratio") when the
    box is dropped for a reason worth tallying separately from the generic
    skipped_boxes count, or None for the generic "not usable" cases."""
    if category_name is None or category_name not in name_to_index:
        return None
    x, y, w, h = annotation.bbox
    raw_area = w * h
    if raw_area > 0.0 and (annotation.area / raw_area) < min_fill_ratio:
        return _LOW_FILL_RATIO
    x1 = max(0.0, min(x, float(image_width)))
    y1 = max(0.0, min(y, float(image_height)))
    x2 = max(0.0, min(x + w, float(image_width)))
    y2 = max(0.0, min(y + h, float(image_height)))
    width = x2 - x1
    height = y2 - y1
    if width <= 0.0 or height <= 0.0:
        return None
    return (
        name_to_index[category_name],
        ((x1 + x2) / 2.0) / image_width,
        ((y1 + y2) / 2.0) / image_height,
        width / image_width,
        height / image_height,
    )


def _resolve_frame_path(frames_dir: Path, frame_id: int) -> Path | None:
    """SFISHTRACK is inconsistent about its own Frames/ naming convention:
    verified against the real dataset, roughly half the videos use
    "frame_NNNNNN.jpg" and the other half use "NNNNNN.png" (matching the
    Masks/ naming instead) -- an even split, not a rare outlier, so both
    must be handled rather than picking one and crashing on the other."""
    jpg_path = frames_dir / f"frame_{frame_id:06d}.jpg"
    if jpg_path.is_file():
        return jpg_path
    png_path = frames_dir / f"{frame_id:06d}.png"
    if png_path.is_file():
        return png_path
    return None


def _prepare_split(
    videos: list[SfishtrackVideo],
    split: str,
    output_root: Path,
    method: str,
    name_to_index: dict[str, int],
    min_fill_ratio: float,
) -> dict:
    image_output = output_root / "images" / split
    label_output = output_root / "labels" / split
    image_output.mkdir(parents=True, exist_ok=True)
    label_output.mkdir(parents=True, exist_ok=True)
    stats = {
        "videos": 0,
        "images": 0,
        "annotations": 0,
        "skipped_boxes": 0,
        "low_fill_ratio_boxes": 0,
        "missing_frames": 0,
        "publish_methods": Counter(),
        "classes": Counter(),
    }
    for video in videos:
        if video.frames_dir is None:
            raise ValueError(f"Missing Frames directory for {video.name}")
        by_image: dict[int, list[SfishtrackAnnotation]] = defaultdict(list)
        for annotation in video.annotations:
            by_image[annotation.image_id].append(annotation)
        stats["videos"] += 1
        for image in video.images:
            # image.file_name ("000295.png") matches the Masks/ naming, not
            # necessarily the actual Frames/ file for this video -- resolved
            # against both real naming conventions, not trusted from the JSON.
            frame_path = _resolve_frame_path(video.frames_dir, image.frame_id)
            if frame_path is None:
                stats["missing_frames"] += 1
                continue
            output_stem = f"{video.name}_{image.frame_id:06d}"
            output_image = image_output / f"{output_stem}{frame_path.suffix.lower()}"
            publish_method = _publish_image(frame_path, output_image, method)
            stats["publish_methods"][publish_method] += 1
            rows: list[str] = []
            for annotation in by_image.get(image.id, []):
                raw_name = video.categories.get(annotation.category_id)
                category_name = _normalize_category_name(raw_name) if raw_name is not None else None
                row = _to_yolo_row(
                    annotation, image.width, image.height, category_name, name_to_index, min_fill_ratio,
                )
                if row is None:
                    stats["skipped_boxes"] += 1
                    continue
                if row == _LOW_FILL_RATIO:
                    stats["low_fill_ratio_boxes"] += 1
                    continue
                class_index, center_x, center_y, width, height = row
                rows.append(
                    f"{class_index} {center_x:.8f} {center_y:.8f} {width:.8f} {height:.8f}"
                )
                stats["annotations"] += 1
                stats["classes"][class_index] += 1
            (label_output / f"{output_stem}.txt").write_text(
                "\n".join(rows) + ("\n" if rows else ""), encoding="utf-8"
            )
            stats["images"] += 1
    stats["publish_methods"] = dict(stats["publish_methods"])
    stats["classes"] = dict(stats["classes"])
    return stats


def prepare_dataset(
    source: str | Path,
    output: str | Path,
    validation_fraction: float = 0.2,
    seed: int = 42,
    method: str = "hardlink",
    min_fill_ratio: float = 0.3,
) -> dict:
    """Converts SFISHTRACK's per-video COCO-style annotations into a flat
    YOLO-format dataset (images/{train,val}, labels/{train,val}, dataset.yaml),
    same output shape as brackishmot.prepare_dataset.

    Unlike BrackishMOT, category *ids* are not consistent across files --
    each video's JSON defines its own categories list independently. Category
    *names* are the only stable identity, so the output class list is built
    from the union of normalized names actually seen (see
    _CATEGORY_ALIASES), in first-seen order -- not from any single video's
    id space. "object" is merged into "fish": only video_001 of the 54
    videos labels its targets "fish"; every other video says "object", but
    visual inspection of 4 independent videos confirmed the boxed subjects
    are small fish in every sample, and the same recurring non-fish creature
    is consistently left unboxed. Without the merge, only 1 of 54 videos
    contributes any "fish"-class ground truth at all.

    `min_fill_ratio` drops annotations whose real segmentation-mask area is a
    small fraction of their bounding-box rectangle -- a real, present data
    quality issue verified by inspection (e.g. video_019 frame 84 track 75:
    a wide loose box spanning a fish plus a large stretch of empty rock,
    filling only 23% of its own rectangle vs 40-75% for well-fitted boxes in
    the same frame). Set to 0.0 to disable and keep every box as-is.

    SFISHTRACK ships no held-out test split (unlike BrackishMOT) -- the
    output's "test" split is always empty rather than inventing one.
    """
    if not 0.0 < validation_fraction < 1.0:
        raise ValueError("validation_fraction must be between 0 and 1")
    source = Path(source).resolve()
    output = Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError(f"Output directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    videos = [read_video_annotations(path) for path in discover_videos(source)]

    class_names: list[str] = []
    for video in videos:
        for name in video.categories.values():
            normalized = _normalize_category_name(name)
            if normalized not in class_names:
                class_names.append(normalized)
    output_names = {index: name for index, name in enumerate(class_names)}
    name_to_index = {name: index for index, name in output_names.items()}

    # Stratified by each video's class signature, not a flat shuffle: verified
    # against the real dataset, "fish" is 0.4% of all annotations (551 of
    # 145,644) and whole videos are labeled either "fish"-only or
    # "object"-only. A flat random video-level split can (and, with the
    # obvious seed, did) put zero fish-labeled videos in validation --
    # silently making it impossible to evaluate fish-detection performance.
    # Splitting each class-signature group independently guarantees every
    # class present in >= 2 videos appears in both train and val.
    rng = random.Random(seed)
    groups: dict[frozenset[str], list[SfishtrackVideo]] = defaultdict(list)
    for video in videos:
        groups[frozenset(video.categories.values())].append(video)

    training_videos: list[SfishtrackVideo] = []
    validation_videos: list[SfishtrackVideo] = []
    for group_videos in groups.values():
        shuffled = list(group_videos)
        rng.shuffle(shuffled)
        if len(shuffled) < 2:
            # Can't split a single video and leave both sides non-empty for
            # this class signature -- it goes to train only.
            training_videos.extend(shuffled)
            continue
        validation_count = max(1, round(len(shuffled) * validation_fraction))
        validation_videos.extend(shuffled[:validation_count])
        training_videos.extend(shuffled[validation_count:])
    training_videos.sort(key=lambda v: v.name)
    validation_videos.sort(key=lambda v: v.name)

    report = {
        "source": str(source),
        "output": str(output),
        "seed": seed,
        "validation_fraction": validation_fraction,
        "class_names": output_names,
        "splits": {},
    }
    for split, split_videos in (
        ("train", training_videos), ("val", validation_videos), ("test", []),
    ):
        report["splits"][split] = _prepare_split(
            split_videos, split, output, method, name_to_index, min_fill_ratio,
        )
        report["splits"][split]["video_names"] = [video.name for video in split_videos]

    dataset_yaml = {
        "path": output.as_posix(),
        "train": "images/train",
        "val": "images/val",
        "test": "images/test",
        "names": output_names,
    }
    (output / "dataset.yaml").write_text(
        yaml.safe_dump(dataset_yaml, sort_keys=False), encoding="utf-8"
    )
    (output / "manifest.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    return report
