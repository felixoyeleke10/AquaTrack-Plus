from __future__ import annotations

from dataclasses import dataclass
from itertools import pairwise
from pathlib import Path
from time import perf_counter

import cv2

from aquatrack.config import AppConfig
from aquatrack.counting import LineCounter
from aquatrack.detectors.base import Detector
from aquatrack.export import write_observations_csv, write_summary_json
from aquatrack.tracking import IoUTracker
from aquatrack.types import Observation, Track


@dataclass(frozen=True, slots=True)
class PipelineResult:
    video_path: Path | None
    csv_path: Path | None
    summary_path: Path | None
    frames_processed: int
    elapsed_seconds: float
    unique_tracks: int
    line_crossings: int


def _draw(frame, tracks: list[Track], counter: LineCounter, config: AppConfig) -> None:
    height, width = frame.shape[:2]
    line_y = int(counter.line_y)
    cv2.line(frame, (0, line_y), (width, line_y), (0, 220, 255), 2)
    for track in tracks:
        if not track.confirmed:
            continue
        box = track.bbox.clipped(width, height)
        color = (30, 210, 70) if not track.counted else (255, 170, 20)
        p1, p2 = (int(box.x1), int(box.y1)), (int(box.x2), int(box.y2))
        cv2.rectangle(frame, p1, p2, color, 2)
        label = f"{track.class_name} #{track.track_id}"
        if config.output.draw_confidence:
            label += f" {track.confidence:.2f}"
        cv2.putText(frame, label, (p1[0], max(18, p1[1] - 7)), cv2.FONT_HERSHEY_SIMPLEX,
                    0.5, color, 2, cv2.LINE_AA)
        if config.output.draw_track_trail:
            trail = track.trail[-config.output.trail_length :]
            for start, end in pairwise(trail):
                cv2.line(frame, tuple(map(int, start)), tuple(map(int, end)), color, 2)
    cv2.putText(frame, f"Crossings: {counter.total}", (16, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 220, 255), 2, cv2.LINE_AA)


def process_video(
    source: str | Path,
    output_dir: str | Path,
    detector: Detector,
    config: AppConfig,
) -> PipelineResult:
    source = Path(source)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    capture = cv2.VideoCapture(str(source))
    if not capture.isOpened():
        raise ValueError(f"Could not open video: {source}")

    fps = float(capture.get(cv2.CAP_PROP_FPS)) or 30.0
    width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    stem = source.stem
    video_path = output_dir / f"{stem}_annotated.mp4" if config.output.save_video else None
    csv_path = output_dir / f"{stem}_observations.csv" if config.output.save_csv else None
    summary_path = output_dir / f"{stem}_summary.json" if config.output.save_json else None

    writer = None
    if video_path:
        writer = cv2.VideoWriter(
            str(video_path), cv2.VideoWriter_fourcc(*"mp4v"), fps, (width, height)
        )
        if not writer.isOpened():
            capture.release()
            raise ValueError(f"Could not create output video: {video_path}")

    tracker = IoUTracker(
        match_iou=config.tracking.match_iou,
        max_missed=config.tracking.max_missed_frames,
        min_hits=config.tracking.min_confirmed_hits,
    )
    counter = LineCounter(height * config.counting.line_position, config.counting.direction)
    observations: list[Observation] = []
    observed_track_ids: set[int] = set()
    frame_index = 0
    started = perf_counter()

    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            detections = detector.detect(frame)
            tracks = tracker.update(detections)
            counter.update(tracks)
            for track in tracks:
                if not track.confirmed:
                    continue
                observed_track_ids.add(track.track_id)
                observations.append(
                    Observation(
                        frame_index=frame_index,
                        timestamp_seconds=frame_index / fps,
                        track_id=track.track_id,
                        class_name=track.class_name,
                        confidence=track.confidence,
                        bbox=track.bbox,
                        counted=track.counted,
                    )
                )
            if writer:
                _draw(frame, tracks, counter, config)
                writer.write(frame)
            frame_index += 1
    finally:
        capture.release()
        if writer:
            writer.release()

    elapsed = perf_counter() - started
    if csv_path:
        write_observations_csv(csv_path, observations)
    if summary_path:
        write_summary_json(
            summary_path, source, observations, counter.total, dict(counter.by_class),
            frame_index, fps,
        )
    return PipelineResult(
        video_path=video_path,
        csv_path=csv_path,
        summary_path=summary_path,
        frames_processed=frame_index,
        elapsed_seconds=elapsed,
        unique_tracks=len(observed_track_ids),
        line_crossings=counter.total,
    )

