from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Any


@dataclass(frozen=True, slots=True)
class BoundingBox:
    x1: float
    y1: float
    x2: float
    y2: float

    @property
    def width(self) -> float:
        return max(0.0, self.x2 - self.x1)

    @property
    def height(self) -> float:
        return max(0.0, self.y2 - self.y1)

    @property
    def area(self) -> float:
        return self.width * self.height

    @property
    def center(self) -> tuple[float, float]:
        return ((self.x1 + self.x2) / 2.0, (self.y1 + self.y2) / 2.0)

    def clipped(self, width: int, height: int) -> BoundingBox:
        return BoundingBox(
            max(0.0, min(self.x1, width - 1.0)),
            max(0.0, min(self.y1, height - 1.0)),
            max(0.0, min(self.x2, width - 1.0)),
            max(0.0, min(self.y2, height - 1.0)),
        )


@dataclass(frozen=True, slots=True)
class Detection:
    bbox: BoundingBox
    confidence: float
    class_id: int
    class_name: str


@dataclass(slots=True)
class Track:
    track_id: int
    bbox: BoundingBox
    class_id: int
    class_name: str
    confidence: float
    hits: int = 1
    missed: int = 0
    age: int = 1
    confirmed: bool = False
    counted: bool = False
    trail: list[tuple[float, float]] = field(default_factory=list)

    def update(self, detection: Detection, min_hits: int) -> None:
        self.bbox = detection.bbox
        self.class_id = detection.class_id
        self.class_name = detection.class_name
        self.confidence = detection.confidence
        self.hits += 1
        self.missed = 0
        self.age += 1
        self.confirmed = self.hits >= min_hits
        self.trail.append(self.bbox.center)

    def mark_missed(self) -> None:
        self.missed += 1
        self.age += 1


@dataclass(frozen=True, slots=True)
class Observation:
    frame_index: int
    timestamp_seconds: float
    track_id: int
    class_name: str
    confidence: float
    bbox: BoundingBox
    counted: bool

    def to_dict(self) -> dict[str, Any]:
        result = asdict(self)
        bbox = result.pop("bbox")
        result.update(bbox)
        return result

