from __future__ import annotations

import csv
import json
from collections import Counter
from pathlib import Path

from aquatrack.types import Observation


def write_observations_csv(path: Path, observations: list[Observation]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = [observation.to_dict() for observation in observations]
    fieldnames = list(rows[0]) if rows else [
        "frame_index", "timestamp_seconds", "track_id", "class_name",
        "confidence", "counted", "x1", "y1", "x2", "y2",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_summary_json(
    path: Path,
    source: Path,
    observations: list[Observation],
    crossing_total: int,
    crossing_by_class: dict[str, int],
    frames_processed: int,
    fps: float,
) -> None:
    unique_tracks = {observation.track_id for observation in observations}
    observed_by_class: Counter[str] = Counter()
    first_class: dict[int, str] = {}
    for observation in observations:
        first_class.setdefault(observation.track_id, observation.class_name)
    observed_by_class.update(first_class.values())
    payload = {
        "source": str(source),
        "frames_processed": frames_processed,
        "source_fps": fps,
        "duration_seconds": frames_processed / fps if fps else 0.0,
        "unique_confirmed_tracks": len(unique_tracks),
        "unique_tracks_by_class": dict(observed_by_class),
        "line_crossing_total": crossing_total,
        "line_crossings_by_class": crossing_by_class,
        "observation_rows": len(observations),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

