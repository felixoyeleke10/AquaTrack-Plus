from __future__ import annotations

import numpy as np

from aquatrack.config import ModelConfig
from aquatrack.types import BoundingBox, Detection


class YoloDetector:
    """Ultralytics YOLO adapter with optional class-name filtering."""

    def __init__(self, config: ModelConfig) -> None:
        from ultralytics import YOLO

        self.config = config
        self.model = YOLO(config.weights)

    def detect(self, frame: np.ndarray) -> list[Detection]:
        device = None if self.config.device == "auto" else self.config.device
        results = self.model.predict(
            source=frame,
            conf=self.config.confidence,
            iou=self.config.iou_threshold,
            device=device,
            verbose=False,
        )
        detections: list[Detection] = []
        allowed = {name.casefold() for name in self.config.classes}
        for result in results:
            names = result.names
            for box in result.boxes:
                class_id = int(box.cls.item())
                class_name = str(names[class_id])
                if allowed and class_name.casefold() not in allowed:
                    continue
                x1, y1, x2, y2 = (float(value) for value in box.xyxy[0].tolist())
                detections.append(
                    Detection(
                        bbox=BoundingBox(x1, y1, x2, y2),
                        confidence=float(box.conf.item()),
                        class_id=class_id,
                        class_name=class_name,
                    )
                )
        return detections

