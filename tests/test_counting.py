from aquatrack.counting import LineCounter
from aquatrack.types import BoundingBox, Track


def make_track(y: float) -> Track:
    return Track(1, BoundingBox(0, y - 5, 10, y + 5), 0, "fish", 0.9, confirmed=True)


def test_counts_crossing_once() -> None:
    counter = LineCounter(line_y=50, direction="down")
    track = make_track(40)
    assert counter.update([track]) == []
    track.bbox = BoundingBox(0, 50, 10, 60)
    assert counter.update([track]) == [1]
    track.bbox = BoundingBox(0, 60, 10, 70)
    assert counter.update([track]) == []
    assert counter.total == 1
    assert counter.by_class["fish"] == 1


def test_direction_filter() -> None:
    counter = LineCounter(line_y=50, direction="up")
    track = make_track(40)
    counter.update([track])
    track.bbox = BoundingBox(0, 50, 10, 60)
    counter.update([track])
    assert counter.total == 0



def test_forgets_tracks_not_seen_for_a_while() -> None:
    counter = LineCounter(line_y=50, forget_after=2)
    counter.update([make_track(40)])
    for _ in range(3):
        counter.update([])
    assert counter._previous_y == {}
