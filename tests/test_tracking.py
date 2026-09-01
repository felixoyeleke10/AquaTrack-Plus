from aquatrack.tracking import IoUTracker, intersection_over_union
from aquatrack.types import BoundingBox, Detection


def detection(x1: float, y1: float, x2: float, y2: float) -> Detection:
    return Detection(BoundingBox(x1, y1, x2, y2), 0.9, 0, "fish")


def test_intersection_over_union() -> None:
    a = BoundingBox(0, 0, 10, 10)
    b = BoundingBox(5, 5, 15, 15)
    assert intersection_over_union(a, b) == 25 / 175


def test_tracker_preserves_identity_for_overlapping_detection() -> None:
    tracker = IoUTracker(match_iou=0.2, max_missed=2, min_hits=2)
    first = tracker.update([detection(10, 10, 30, 30)])
    second = tracker.update([detection(12, 10, 32, 30)])
    assert first[0].track_id == second[0].track_id
    assert second[0].confirmed


def test_tracker_expires_missing_track() -> None:
    tracker = IoUTracker(max_missed=1, min_hits=1)
    tracker.update([detection(0, 0, 10, 10)])
    tracker.update([])
    tracker.update([])
    assert tracker.tracks == []

