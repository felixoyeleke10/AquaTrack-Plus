"""Detect and track fish over any media AquaTrack+ can review.

Run by the desktop app as ``python -m aquatrack.analyze --source PATH ...``.
``PATH`` is what Review opens: a video file, a video sequence folder
(``sequence.json``) or a BrackishMOT image sequence (``seqinfo.ini``).

Frames are numbered exactly as the app numbers them: 1-based and continuous
across the clips of a video sequence. Track IDs stay unique across clips (the
tracker restarts at each clip boundary, because objects cannot be followed
across a cut).

Progress is written to stdout as one JSON object per line, so the app can show
it without parsing log text::

    {"event": "start", "total": 1067, "model": "...", "device": "cuda:0"}
    {"event": "stage", "text": "Loading the model"}
    {"event": "progress", "done": 120, "total": 1067, "fps": 41.2}
    {"event": "done", "observations": 8123, "tracks": 57, "output": "..."}
    {"event": "error", "message": "..."}

Results are written as compact JSON (see ``write_results``).
"""

from __future__ import annotations

import argparse
import configparser
import json
import math
import sys
import tempfile
import time
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Protocol

import numpy as np

from aquatrack.tracking import IoUTracker
from aquatrack.types import BoundingBox, Detection

RESULTS_FORMAT = "aquatrack-analysis"
GENERIC_CLASS_NAMES = {"", "object", "objects", "target", "animal", "0"}


def emit(event: str, **payload: Any) -> None:
    print(json.dumps({"event": event, **payload}), flush=True)


# ---------------------------------------------------------------------------
# Media


@dataclass(slots=True)
class Clip:
    path: Path
    frames: int


@dataclass(slots=True)
class Media:
    kind: str  # "video", "video-sequence" or "image-sequence"
    name: str
    clips: list[Clip] = field(default_factory=list)
    images: list[Path] = field(default_factory=list)

    @property
    def total_frames(self) -> int:
        if self.kind == "image-sequence":
            return len(self.images)
        return sum(clip.frames for clip in self.clips)


def _video_frame_count(path: Path) -> int:
    import cv2

    capture = cv2.VideoCapture(str(path))
    try:
        return max(0, int(capture.get(cv2.CAP_PROP_FRAME_COUNT)))
    finally:
        capture.release()


def open_media(source: Path) -> Media:
    source = source.resolve()
    if source.is_dir() and (source / "seqinfo.ini").is_file():
        parser = configparser.ConfigParser()
        parser.read(source / "seqinfo.ini", encoding="utf-8")
        section = parser["Sequence"]
        image_dir = source / section.get("imDir", "img1")
        extension = section.get("imExt", ".jpg")
        length = int(section.get("seqLength", "0"))
        images = [image_dir / f"{index:06d}{extension}" for index in range(1, length + 1)]
        return Media("image-sequence", section.get("name", source.name), images=images)
    if source.is_dir() and (source / "sequence.json").is_file():
        manifest = json.loads((source / "sequence.json").read_text(encoding="utf-8"))
        if manifest.get("format") != "aquatrack-video-sequence":
            raise ValueError(f"{source / 'sequence.json'} is not an AquaTrack video sequence")
        clips = []
        for entry in manifest.get("clips", []):
            path = Path(entry["path"])
            path = path if path.is_absolute() else (source / path)
            path = path.resolve()
            if not path.is_file():
                raise FileNotFoundError(f"A video in this sequence is missing: {path}")
            frames = int(entry.get("frames") or 0) or _video_frame_count(path)
            clips.append(Clip(path, frames))
        if not clips:
            raise ValueError("The video sequence lists no videos")
        return Media("video-sequence", manifest.get("name") or source.name, clips=clips)
    if source.is_file():
        return Media("video", source.stem, clips=[Clip(source, _video_frame_count(source))])
    raise FileNotFoundError(f"Nothing reviewable at {source}")


def iterate_frames(media: Media) -> Iterator[tuple[int, int, np.ndarray]]:
    """Yields (global 1-based frame, clip index, BGR image)."""
    import cv2

    if media.kind == "image-sequence":
        for index, path in enumerate(media.images, start=1):
            image = cv2.imread(str(path), cv2.IMREAD_COLOR)
            if image is None:
                raise OSError(f"Could not read {path}")
            yield index, 0, image
        return

    first = 1
    for clip_index, clip in enumerate(media.clips):
        capture = cv2.VideoCapture(str(clip.path))
        if not capture.isOpened():
            raise OSError(f"Could not open {clip.path}")
        local = 0
        try:
            # Read exactly as many frames as the app shows for this clip, so
            # frame numbers line up even if a container miscounts.
            last = None
            while local < clip.frames:
                ok, image = capture.read()
                if not ok:
                    if last is None:
                        break
                    image = last  # the app holds the last frame too
                last = image
                yield first + local, clip_index, image
                local += 1
        finally:
            capture.release()
        first += clip.frames


# ---------------------------------------------------------------------------
# Detection + tracking


@dataclass(slots=True)
class TrackedBox:
    track_id: int
    class_name: str
    confidence: float
    x1: float
    y1: float
    x2: float
    y2: float


class FrameTracker(Protocol):
    def update(self, image: np.ndarray) -> list[TrackedBox]: ...
    def reset(self) -> None: ...


def _resolve_device(requested: str) -> str:
    if requested != "auto":
        return requested
    try:
        import torch

        return "cuda:0" if torch.cuda.is_available() else "cpu"
    except ImportError:  # pragma: no cover - torch is a hard dependency in practice
        return "cpu"


def _load_ultralytics(weights: str):
    from ultralytics import RTDETR, YOLO

    model = YOLO(weights)
    # RT-DETR checkpoints load into YOLO() but need RT-DETR's own predictor.
    if type(model.model).__name__.startswith("RTDETR"):
        model = RTDETR(weights)
    return model


class UltralyticsByteTrack:
    """Ultralytics detector with its ByteTrack implementation."""

    def __init__(self, weights: str, args: argparse.Namespace, device: str) -> None:
        self.model = _load_ultralytics(weights)
        self.names = self.model.names
        self.args = args
        self.device = device
        self.allowed = {name.casefold() for name in args.classes}
        # ByteTrack associates low-scoring detections with existing tracks,
        # so the detector runs at a low floor and the user's threshold
        # decides which detections may start or confirm a track.
        config = {
            "tracker_type": "bytetrack",
            "track_high_thresh": float(args.confidence),
            "track_low_thresh": min(0.1, float(args.confidence)),
            "new_track_thresh": float(args.confidence),
            "track_buffer": int(args.max_missed),
            "match_thresh": 0.8,
            "fuse_score": True,
        }
        handle = tempfile.NamedTemporaryFile(
            "w", suffix=".yaml", prefix="aquatrack_bytetrack_", delete=False, encoding="utf-8"
        )
        with handle:
            for key, value in config.items():
                handle.write(f"{key}: {value}\n")
        self.tracker_config = handle.name
        self._persist = False

    def reset(self) -> None:
        self._persist = False  # a fresh tracker for the next clip

    def update(self, image: np.ndarray) -> list[TrackedBox]:
        results = self.model.track(
            source=image,
            persist=self._persist,
            tracker=self.tracker_config,
            conf=min(0.1, float(self.args.confidence)),
            iou=float(self.args.iou),
            imgsz=int(self.args.imgsz),
            device=self.device,
            verbose=False,
        )
        self._persist = True
        boxes: list[TrackedBox] = []
        for result in results:
            if result.boxes is None or result.boxes.id is None:
                continue
            ids = result.boxes.id.int().cpu().tolist()
            classes = result.boxes.cls.int().cpu().tolist()
            confidences = result.boxes.conf.cpu().tolist()
            coordinates = result.boxes.xyxy.cpu().tolist()
            for track_id, class_id, confidence, (x1, y1, x2, y2) in zip(
                ids, classes, confidences, coordinates, strict=True
            ):
                name = str(self.names.get(class_id, class_id))
                if self.allowed and name.casefold() not in self.allowed:
                    continue
                boxes.append(TrackedBox(track_id, name, confidence, x1, y1, x2, y2))
        return boxes

    def close(self) -> None:
        Path(self.tracker_config).unlink(missing_ok=True)


class DetectorWithIoUTracker:
    """Any per-frame detector followed by the transparent IoU tracker."""

    def __init__(self, detect, args: argparse.Namespace) -> None:
        self.detect = detect
        self.args = args
        self.reset()

    def reset(self) -> None:
        self.tracker = IoUTracker(
            match_iou=float(self.args.match_iou),
            max_missed=int(self.args.max_missed),
            min_hits=int(self.args.min_hits),
        )

    def update(self, image: np.ndarray) -> list[TrackedBox]:
        tracks = self.tracker.update(self.detect(image))
        return [
            TrackedBox(
                track.track_id, track.class_name, track.confidence,
                track.bbox.x1, track.bbox.y1, track.bbox.x2, track.bbox.y2,
            )
            for track in tracks
            if track.confirmed
        ]

    def close(self) -> None:
        pass


def ultralytics_detector(weights: str, args: argparse.Namespace, device: str):
    model = _load_ultralytics(weights)
    allowed = {name.casefold() for name in args.classes}

    def detect(image: np.ndarray) -> list[Detection]:
        detections = []
        for result in model.predict(
            source=image, conf=float(args.confidence), iou=float(args.iou),
            imgsz=int(args.imgsz), device=device, verbose=False,
        ):
            for box in result.boxes:
                class_id = int(box.cls.item())
                name = str(result.names[class_id])
                if allowed and name.casefold() not in allowed:
                    continue
                x1, y1, x2, y2 = (float(value) for value in box.xyxy[0].tolist())
                detections.append(
                    Detection(BoundingBox(x1, y1, x2, y2), float(box.conf.item()), class_id, name)
                )
        return detections

    return detect


def torchvision_detector(weights: str, args: argparse.Namespace, device: str):
    """Faster R-CNN (ResNet50-FPN-v2) trained with tools/train_faster_rcnn.py."""
    import torch
    from torchvision.models.detection import fasterrcnn_resnet50_fpn_v2
    from torchvision.models.detection.faster_rcnn import FastRCNNPredictor

    state = torch.load(weights, map_location=device, weights_only=True)
    if isinstance(state, dict) and "model" in state and isinstance(state["model"], dict):
        state = state["model"]
    class_count = int(state["roi_heads.box_predictor.cls_score.weight"].shape[0])
    names = list(args.class_names) or ["fish"]
    model = fasterrcnn_resnet50_fpn_v2(weights=None, weights_backbone=None,
                                      min_size=int(args.imgsz), max_size=int(args.imgsz))
    model.roi_heads.box_predictor = FastRCNNPredictor(
        model.roi_heads.box_predictor.cls_score.in_features, class_count
    )
    model.load_state_dict(state)
    model.to(device).eval()

    @torch.no_grad()
    def detect(image: np.ndarray) -> list[Detection]:
        rgb = np.ascontiguousarray(image[:, :, ::-1])
        tensor = torch.from_numpy(rgb).permute(2, 0, 1).float().div(255).to(device)
        prediction = model([tensor])[0]
        detections = []
        for box, score, label in zip(
            prediction["boxes"].cpu().tolist(), prediction["scores"].cpu().tolist(),
            prediction["labels"].cpu().tolist(), strict=True,
        ):
            if score < float(args.confidence):
                continue
            index = int(label) - 1  # 0 is background
            name = names[index] if 0 <= index < len(names) else "fish"
            detections.append(Detection(BoundingBox(*box), float(score), index, name))
        return detections

    return detect


def build_tracker(args: argparse.Namespace, device: str):
    backend = args.backend.lower()
    if "torchvision" in backend or "faster" in backend:
        return DetectorWithIoUTracker(torchvision_detector(args.weights, args, device), args)
    if args.tracker == "bytetrack":
        return UltralyticsByteTrack(args.weights, args, device)
    return DetectorWithIoUTracker(ultralytics_detector(args.weights, args, device), args)


# ---------------------------------------------------------------------------
# Results


def display_class_name(name: str) -> str:
    """Fish-dataset models often call their only class "object" or "0"."""
    cleaned = name.strip()
    if cleaned.casefold() in GENERIC_CLASS_NAMES or cleaned.casefold() == "fish":
        return "Fish"
    return cleaned[:1].upper() + cleaned[1:]


def write_results(path: Path, media: Media, args: argparse.Namespace,
                  rows: list[list[float]], classes: list[str], elapsed: float) -> None:
    """Rows are [frame, track, class index, confidence, x1, y1, x2, y2]."""
    payload = {
        "format": RESULTS_FORMAT,
        "version": 1,
        "source": str(Path(args.source).resolve()),
        "name": media.name,
        "frames": media.total_frames,
        "model": {"name": args.model_name or Path(args.weights).name, "weights": args.weights,
                  "backend": args.backend},
        "tracker": args.tracker,
        "settings": {"confidence": args.confidence, "iou": args.iou, "imgsz": args.imgsz},
        "elapsedSeconds": round(elapsed, 2),
        "classes": classes,
        "columns": ["frame", "track", "class", "confidence", "x1", "y1", "x2", "y2"],
        "observations": rows,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".part")
    temporary.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")
    temporary.replace(path)


def run(args: argparse.Namespace) -> int:
    media = open_media(Path(args.source))
    total = media.total_frames
    device = _resolve_device(args.device)
    emit("start", total=total, name=media.name, device=device,
         model=args.model_name or Path(args.weights).name, clips=len(media.clips) or 1)

    emit("stage", text="Loading the model")
    tracker = build_tracker(args, device)
    emit("stage", text="Detecting and tracking")
    rows: list[list[float]] = []
    classes: list[str] = []
    class_index: dict[str, int] = {}
    track_offset = 0
    clip_max_id = 0
    current_clip = 0
    started = time.perf_counter()
    last_report = 0.0
    done = 0
    try:
        for frame, clip_index, image in iterate_frames(media):
            if clip_index != current_clip:
                tracker.reset()
                track_offset += clip_max_id
                clip_max_id = 0
                current_clip = clip_index
            height, width = image.shape[:2]
            for box in tracker.update(image):
                clip_max_id = max(clip_max_id, int(box.track_id))
                name = display_class_name(box.class_name)
                if name not in class_index:
                    class_index[name] = len(classes)
                    classes.append(name)
                x1 = max(0.0, min(float(box.x1), width - 1.0))
                y1 = max(0.0, min(float(box.y1), height - 1.0))
                x2 = max(0.0, min(float(box.x2), width - 1.0))
                y2 = max(0.0, min(float(box.y2), height - 1.0))
                if x2 - x1 < 1.0 or y2 - y1 < 1.0 or not math.isfinite(box.confidence):
                    continue
                rows.append([frame, track_offset + int(box.track_id), class_index[name],
                             round(float(box.confidence), 4), round(x1, 1), round(y1, 1),
                             round(x2, 1), round(y2, 1)])
            done = frame
            now = time.perf_counter()
            if now - last_report >= 0.4 or frame == total:
                last_report = now
                emit("progress", done=done, total=total,
                     fps=round(done / max(now - started, 1e-6), 1))
    finally:
        close = getattr(tracker, "close", None)
        if close:
            close()

    elapsed = time.perf_counter() - started
    output = Path(args.output)
    write_results(output, media, args, rows, classes, elapsed)
    emit("done", observations=len(rows), tracks=len({row[1] for row in rows}),
         frames=done, seconds=round(elapsed, 1), output=str(output))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="aquatrack.analyze", description=__doc__.split("\n")[0])
    parser.add_argument("--source", required=True, help="Video, video sequence or image sequence")
    parser.add_argument("--weights", required=True)
    parser.add_argument("--backend", default="ultralytics")
    parser.add_argument("--model-name", default="")
    parser.add_argument("--output", required=True)
    parser.add_argument("--confidence", type=float, default=0.35)
    parser.add_argument("--iou", type=float, default=0.7)
    parser.add_argument("--imgsz", type=int, default=640)
    parser.add_argument("--device", default="auto")
    parser.add_argument("--tracker", choices=["bytetrack", "iou"], default="bytetrack")
    parser.add_argument("--match-iou", type=float, default=0.3)
    parser.add_argument("--min-hits", type=int, default=3)
    parser.add_argument("--max-missed", type=int, default=30)
    parser.add_argument("--classes", nargs="*", default=[],
                        help="Keep only these detector classes (case-insensitive)")
    parser.add_argument("--class-names", nargs="*", default=[],
                        help="Names for a torchvision model's classes, in index order")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return run(args)
    except KeyboardInterrupt:
        emit("error", message="Cancelled")
        return 130
    except Exception as error:  # reported to the app, which shows it verbatim
        emit("error", message=f"{type(error).__name__}: {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
