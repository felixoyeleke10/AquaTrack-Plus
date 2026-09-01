from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml


@dataclass(slots=True)
class ModelConfig:
    weights: str = "yolo11n.pt"
    confidence: float = 0.30
    iou_threshold: float = 0.50
    device: str = "auto"
    classes: list[str] = field(default_factory=lambda: ["fish"])


@dataclass(slots=True)
class TrackingConfig:
    match_iou: float = 0.30
    max_missed_frames: int = 20
    min_confirmed_hits: int = 3


@dataclass(slots=True)
class CountingConfig:
    line_position: float = 0.50
    direction: str = "any"


@dataclass(slots=True)
class OutputConfig:
    draw_confidence: bool = True
    draw_track_trail: bool = True
    trail_length: int = 24
    save_video: bool = True
    save_csv: bool = True
    save_json: bool = True


@dataclass(slots=True)
class AppConfig:
    model: ModelConfig = field(default_factory=ModelConfig)
    tracking: TrackingConfig = field(default_factory=TrackingConfig)
    counting: CountingConfig = field(default_factory=CountingConfig)
    output: OutputConfig = field(default_factory=OutputConfig)


def _construct(cls: type, values: dict[str, Any] | None):
    return cls(**(values or {}))


def load_config(path: str | Path | None = None) -> AppConfig:
    if path is None:
        return AppConfig()
    with Path(path).open("r", encoding="utf-8") as handle:
        raw = yaml.safe_load(handle) or {}
    config = AppConfig(
        model=_construct(ModelConfig, raw.get("model")),
        tracking=_construct(TrackingConfig, raw.get("tracking")),
        counting=_construct(CountingConfig, raw.get("counting")),
        output=_construct(OutputConfig, raw.get("output")),
    )
    if not 0.0 < config.counting.line_position < 1.0:
        raise ValueError("counting.line_position must be between 0 and 1")
    if config.counting.direction not in {"any", "up", "down"}:
        raise ValueError("counting.direction must be any, up, or down")
    return config

