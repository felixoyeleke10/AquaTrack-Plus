from pathlib import Path

import pytest

from aquatrack.datasets.brackishmot import (
    MotAnnotation,
    parse_annotations,
    read_sequence_info,
    to_yolo_row,
)


def make_sequence(tmp_path: Path) -> Path:
    sequence = tmp_path / "brackishMOT-01"
    (sequence / "img1").mkdir(parents=True)
    (sequence / "gt").mkdir()
    (sequence / "seqinfo.ini").write_text(
        "[Sequence]\nname=brackishMOT-01\nimDir=img1\nframeRate=15\n"
        "seqLength=1\nimWidth=100\nimHeight=50\nimExt=.jpg\n",
        encoding="utf-8",
    )
    (sequence / "gt" / "gt.txt").write_text(
        "1,7,10,5,20,10,1,5,0.8\n", encoding="utf-8"
    )
    return sequence


def test_reads_sequence_metadata(tmp_path: Path) -> None:
    info = read_sequence_info(make_sequence(tmp_path))
    assert info.name == "brackishMOT-01"
    assert info.width == 100
    assert info.height == 50
    assert info.frame_rate == 15


def test_parses_mot_annotation(tmp_path: Path) -> None:
    annotations = parse_annotations(make_sequence(tmp_path))
    assert annotations == [MotAnnotation(1, 7, 10, 5, 20, 10, 1, 5, 0.8)]


def test_converts_to_normalized_yolo_row() -> None:
    annotation = MotAnnotation(1, 7, 10, 5, 20, 10, 1, 5, 1)
    row = to_yolo_row(annotation, image_width=100, image_height=50)
    assert row == pytest.approx((4, 0.2, 0.2, 0.2, 0.2))


def test_fish_only_mapping_merges_fish_classes() -> None:
    large = MotAnnotation(1, 1, 0, 0, 10, 10, 1, 1, 1)
    small = MotAnnotation(1, 2, 0, 0, 10, 10, 1, 5, 1)
    crab = MotAnnotation(1, 3, 0, 0, 10, 10, 1, 2, 1)
    mapping = {1: 0, 5: 0}
    assert to_yolo_row(large, 20, 20, mapping)[0] == 0
    assert to_yolo_row(small, 20, 20, mapping)[0] == 0
    assert to_yolo_row(crab, 20, 20, mapping) is None


def test_clips_boxes_to_image_bounds() -> None:
    annotation = MotAnnotation(1, 1, -10, -5, 30, 20, 1, 1, 1)
    row = to_yolo_row(annotation, 100, 50)
    assert row == pytest.approx((0, 0.1, 0.15, 0.2, 0.3))

