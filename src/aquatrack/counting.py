from __future__ import annotations

from collections import Counter

from aquatrack.types import Track


class LineCounter:
    def __init__(self, line_y: float, direction: str = "any") -> None:
        if direction not in {"any", "up", "down"}:
            raise ValueError("direction must be any, up, or down")
        self.line_y = line_y
        self.direction = direction
        self.total = 0
        self.by_class: Counter[str] = Counter()
        self._previous_y: dict[int, float] = {}

    def update(self, tracks: list[Track]) -> list[int]:
        crossed: list[int] = []
        for track in tracks:
            if not track.confirmed:
                continue
            current_y = track.bbox.center[1]
            previous_y = self._previous_y.get(track.track_id)
            self._previous_y[track.track_id] = current_y
            if previous_y is None or track.counted:
                continue
            moved_down = previous_y < self.line_y <= current_y
            moved_up = previous_y > self.line_y >= current_y
            valid = (
                (self.direction == "any" and (moved_up or moved_down))
                or (self.direction == "up" and moved_up)
                or (self.direction == "down" and moved_down)
            )
            if valid:
                track.counted = True
                self.total += 1
                self.by_class[track.class_name] += 1
                crossed.append(track.track_id)
        return crossed

