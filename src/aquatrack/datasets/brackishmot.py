from __future__ import annotations

import configparser
import json
import os
import random
import shutil
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

import yaml


BRACKISHMOT_CLASSES = {
    1: "fish",
    2: "crab",
    3: "shrimp",
    4: "starfish",
    5: "small fish",
    6: "jellyfish",
}


@dataclass(frozen=True, slots=True)
class SequenceInfo:
    name: str
    path: Path
    image_dir: Path
    frame_rate: float
    length: int
    width: int
    height: int
    extension: str


@dataclass(frozen=True, slots=True)
class MotAnnotation:
    frame: int
    track_id: int
    left: float
    top: float
    width: float
    height: float
    confidence: float
    class_id: int
    visibility: float


def read_sequence_info(sequence_dir: str | Path) -> SequenceInfo:
    sequence_dir = Path(sequence_dir)
    ini_path = sequence_dir / "seqinfo.ini"
    parser = configparser.ConfigParser()
    if not parser.read(ini_path, encoding="utf-8") or "Sequence" not in parser:
        raise ValueError(f"Missing or invalid sequence metadata: {ini_path}")
    section = parser["Sequence"]
    image_dir = sequence_dir / section.get("imDir", "img1")
    info = SequenceInfo(
        name=section.get("name", sequence_dir.name),
        path=sequence_dir,
        image_dir=image_dir,
        frame_rate=section.getfloat("frameRate", fallback=15.0),
        length=section.getint("seqLength"),
        width=section.getint("imWidth"),
        height=section.getint("imHeight"),
        extension=section.get("imExt", ".jpg"),
    )
    if info.width <= 0 or info.height <= 0 or info.length <= 0:
        raise ValueError(f"Invalid dimensions or length in {ini_path}")
    if not image_dir.is_dir():
        raise ValueError(f"Missing image directory: {image_dir}")
    return info


def parse_annotations(sequence_dir: str | Path) -> list[MotAnnotation]:
    gt_path = Path(sequence_dir) / "gt" / "gt.txt"
    if not gt_path.is_file():
        raise ValueError(f"Missing ground truth: {gt_path}")
    annotations: list[MotAnnotation] = []
    with gt_path.open("r", encoding="utf-8-sig") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue
            fields = [field.strip() for field in line.split(",")]
            if len(fields) != 9:
                raise ValueError(f"Expected 9 fields at {gt_path}:{line_number}")
            annotation = MotAnnotation(
                frame=int(fields[0]),
                track_id=int(fields[1]),
                left=float(fields[2]),
                top=float(fields[3]),
                width=float(fields[4]),
                height=float(fields[5]),
                confidence=float(fields[6]),
                class_id=int(fields[7]),
                visibility=float(fields[8]),
            )
            if annotation.class_id not in BRACKISHMOT_CLASSES:
                raise ValueError(
                    f"Unknown class {annotation.class_id} at {gt_path}:{line_number}"
                )
            annotations.append(annotation)
    return annotations


def discover_sequences(dataset_root: str | Path) -> dict[str, list[Path]]:
    dataset_root = Path(dataset_root)
    discovered: dict[str, list[Path]] = {}
    for split in ("train", "test"):
        split_dir = dataset_root / split
        if not split_dir.is_dir():
            raise ValueError(f"Missing BrackishMOT {split} directory: {split_dir}")
        sequences = sorted(
            path for path in split_dir.iterdir() if path.is_dir() and (path / "seqinfo.ini").is_file()
        )
        if not sequences:
            raise ValueError(f"No sequences found in {split_dir}")
        discovered[split] = sequences
    return discovered


def to_yolo_row(
    annotation: MotAnnotation,
    image_width: int,
    image_height: int,
    class_map: dict[int, int] | None = None,
) -> tuple[int, float, float, float, float] | None:
    class_map = class_map or {class_id: class_id - 1 for class_id in BRACKISHMOT_CLASSES}
    if annotation.class_id not in class_map:
        return None
    x1 = max(0.0, min(annotation.left, float(image_width)))
    y1 = max(0.0, min(annotation.top, float(image_height)))
    x2 = max(0.0, min(annotation.left + annotation.width, float(image_width)))
    y2 = max(0.0, min(annotation.top + annotation.height, float(image_height)))
    width = x2 - x1
    height = y2 - y1
    if width <= 0.0 or height <= 0.0:
        return None
    return (
        class_map[annotation.class_id],
        ((x1 + x2) / 2.0) / image_width,
        ((y1 + y2) / 2.0) / image_height,
        width / image_width,
        height / image_height,
    )


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


def _prepare_split(
    sequences: Iterable[Path],
    split: str,
    output_root: Path,
    method: str,
    class_map: dict[int, int],
) -> dict:
    image_output = output_root / "images" / split
    label_output = output_root / "labels" / split
    image_output.mkdir(parents=True, exist_ok=True)
    label_output.mkdir(parents=True, exist_ok=True)
    stats = {
        "sequences": 0,
        "images": 0,
        "annotations": 0,
        "skipped_boxes": 0,
        "publish_methods": Counter(),
        "classes": Counter(),
    }
    for sequence_path in sequences:
        info = read_sequence_info(sequence_path)
        by_frame: dict[int, list[MotAnnotation]] = defaultdict(list)
        for annotation in parse_annotations(sequence_path):
            by_frame[annotation.frame].append(annotation)
        image_paths = sorted(info.image_dir.glob(f"*{info.extension}"))
        if not image_paths:
            raise ValueError(f"No {info.extension} frames in {info.image_dir}")
        stats["sequences"] += 1
        for image_path in image_paths:
            try:
                frame = int(image_path.stem)
            except ValueError as exc:
                raise ValueError(f"Frame filename is not numeric: {image_path}") from exc
            output_stem = f"{info.name}_{frame:06d}"
            output_image = image_output / f"{output_stem}{image_path.suffix.lower()}"
            publish_method = _publish_image(image_path, output_image, method)
            stats["publish_methods"][publish_method] += 1
            rows: list[str] = []
            for annotation in by_frame.get(frame, []):
                row = to_yolo_row(annotation, info.width, info.height, class_map)
                if row is None:
                    stats["skipped_boxes"] += 1
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
    fish_only: bool = False,
) -> dict:
    if not 0.0 < validation_fraction < 1.0:
        raise ValueError("validation_fraction must be between 0 and 1")
    source = Path(source).resolve()
    output = Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError(f"Output directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    discovered = discover_sequences(source)
    train_sequences = list(discovered["train"])
    rng = random.Random(seed)
    rng.shuffle(train_sequences)
    validation_count = max(1, round(len(train_sequences) * validation_fraction))
    validation_sequences = sorted(train_sequences[:validation_count])
    training_sequences = sorted(train_sequences[validation_count:])
    test_sequences = discovered["test"]
    if fish_only:
        class_map = {1: 0, 5: 0}
        output_names = {0: "fish"}
    else:
        class_map = {class_id: class_id - 1 for class_id in BRACKISHMOT_CLASSES}
        output_names = {index: name for index, name in enumerate(BRACKISHMOT_CLASSES.values())}

    report = {
        "source": str(source),
        "output": str(output),
        "seed": seed,
        "validation_fraction": validation_fraction,
        "fish_only": fish_only,
        "class_names": output_names,
        "splits": {},
    }
    for split, sequences in (
        ("train", training_sequences),
        ("val", validation_sequences),
        ("test", test_sequences),
    ):
        report["splits"][split] = _prepare_split(
            sequences, split, output, method, class_map
        )
        report["splits"][split]["sequence_names"] = [path.name for path in sequences]

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

