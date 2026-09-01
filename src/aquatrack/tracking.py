from __future__ import annotations

from aquatrack.types import BoundingBox, Detection, Track


def intersection_over_union(a: BoundingBox, b: BoundingBox) -> float:
    x1, y1 = max(a.x1, b.x1), max(a.y1, b.y1)
    x2, y2 = min(a.x2, b.x2), min(a.y2, b.y2)
    intersection = max(0.0, x2 - x1) * max(0.0, y2 - y1)
    union = a.area + b.area - intersection
    return intersection / union if union > 0.0 else 0.0


class IoUTracker:
    """Small deterministic tracker used as a transparent baseline.

    It deliberately avoids hidden state from an external tracking framework.
    Replace it with ByteTrack/BoT-SORT behind the same update interface for
    production comparisons.
    """

    def __init__(self, match_iou: float = 0.3, max_missed: int = 20, min_hits: int = 3):
        self.match_iou = match_iou
        self.max_missed = max_missed
        self.min_hits = min_hits
        self._tracks: dict[int, Track] = {}
        self._next_id = 1

    @property
    def tracks(self) -> list[Track]:
        return list(self._tracks.values())

    def update(self, detections: list[Detection]) -> list[Track]:
        candidates: list[tuple[float, int, int]] = []
        for track_id, track in self._tracks.items():
            for detection_index, detection in enumerate(detections):
                if detection.class_id != track.class_id:
                    continue
                score = intersection_over_union(track.bbox, detection.bbox)
                if score >= self.match_iou:
                    candidates.append((score, track_id, detection_index))

        matched_tracks: set[int] = set()
        matched_detections: set[int] = set()
        for _, track_id, detection_index in sorted(candidates, reverse=True):
            if track_id in matched_tracks or detection_index in matched_detections:
                continue
            self._tracks[track_id].update(detections[detection_index], self.min_hits)
            matched_tracks.add(track_id)
            matched_detections.add(detection_index)

        for track_id, track in list(self._tracks.items()):
            if track_id not in matched_tracks:
                track.mark_missed()
            if track.missed > self.max_missed:
                del self._tracks[track_id]

        for index, detection in enumerate(detections):
            if index in matched_detections:
                continue
            track = Track(
                track_id=self._next_id,
                bbox=detection.bbox,
                class_id=detection.class_id,
                class_name=detection.class_name,
                confidence=detection.confidence,
                confirmed=self.min_hits <= 1,
                trail=[detection.bbox.center],
            )
            self._tracks[self._next_id] = track
            self._next_id += 1

        return [track for track in self._tracks.values() if track.missed == 0]

