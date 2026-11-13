from __future__ import annotations

from collections import Counter

from aquatrack.types import Track


class LineCounter:
    def __init__(self, line_y: float, direction: str = "any", forget_after: int = 1000) -> None:
        if direction not in {"any", "up", "down"}:
            raise ValueError("direction must be any, up, or down")
        if forget_after < 1:
            raise ValueError("forget_after must be at least 1")
        self.line_y = line_y
        self.direction = direction
        self.total = 0
        self.by_class: Counter[str] = Counter()
        # Last centre y and the update it was seen on, per track. Tracks that
        # have not been seen for `forget_after` updates are dropped, so a long
        # video does not keep every track id it ever produced.
        self.forget_after = forget_after
        self._previous_y: dict[int, float] = {}
        self._last_seen: dict[int, int] = {}
        self._updates = 0

    def update(self, tracks: list[Track]) -> list[int]:
        self._updates += 1
        crossed: list[int] = []
        for track in tracks:
            if not track.confirmed:
                continue
            current_y = track.bbox.center[1]
            previous_y = self._previous_y.get(track.track_id)
            self._previous_y[track.track_id] = current_y
            self._last_seen[track.track_id] = self._updates
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
        stale = [
            track_id
            for track_id, seen in self._last_seen.items()
            if self._updates - seen > self.forget_after
        ]
        for track_id in stale:
            del self._last_seen[track_id]
            del self._previous_y[track_id]
        return crossed

