from __future__ import annotations

import json
from pathlib import Path

import cv2
import numpy as np
import pytest

from aquatrack import analyze


def _write_video(path: Path, frames: int, size=(64, 48), fps=10.0) -> None:
    writer = cv2.VideoWriter(str(path), cv2.VideoWriter_fourcc(*"mp4v"), fps, size)
    assert writer.isOpened()
    for index in range(frames):
        image = np.full((size[1], size[0], 3), index * 7 % 255, dtype=np.uint8)
        writer.write(image)
    writer.release()


class _StubTracker:
    """One fish per frame; the tracker restarts its ids at 1 on reset."""

    def __init__(self) -> None:
        self.resets = 0

    def reset(self) -> None:
        self.resets += 1

    def update(self, image: np.ndarray) -> list[analyze.TrackedBox]:
        return [analyze.TrackedBox(1, "object", 0.9, 5, 5, 25, 20)]


def _args(source: Path, output: Path) -> list[str]:
    return ["--source", str(source), "--weights", "stub.pt", "--output", str(output),
            "--model-name", "Stub detector"]


def test_video_sequence_frames_continue_and_tracks_stay_unique(tmp_path, monkeypatch, capsys):
    _write_video(tmp_path / "a.mp4", 6)
    _write_video(tmp_path / "b.mp4", 4)
    folder = tmp_path / "media" / "Transect"
    folder.mkdir(parents=True)
    (folder / "sequence.json").write_text(json.dumps({
        "format": "aquatrack-video-sequence", "version": 1, "name": "Transect",
        "clips": [{"path": "../../a.mp4", "frames": 6}, {"path": "../../b.mp4", "frames": 4}],
    }), encoding="utf-8")
    stub = _StubTracker()
    monkeypatch.setattr(analyze, "build_tracker", lambda args, device: stub)
    output = tmp_path / "out.json"

    assert analyze.main(_args(folder, output)) == 0

    result = json.loads(output.read_text(encoding="utf-8"))
    assert result["format"] == "aquatrack-analysis"
    assert result["frames"] == 10
    assert result["classes"] == ["Fish"], "a generic class name is shown as Fish"
    frames = [row[0] for row in result["observations"]]
    assert frames == list(range(1, 11)), "frames are 1-based and continue across clips"
    tracks = {row[0]: row[1] for row in result["observations"]}
    assert tracks[6] == 1 and tracks[7] == 2, "the second clip's track does not reuse id 1"
    assert stub.resets == 1, "the tracker restarts at the clip boundary"

    events = [json.loads(line) for line in capsys.readouterr().out.splitlines() if line.startswith("{")]
    assert events[0]["event"] == "start" and events[0]["total"] == 10
    assert events[-1]["event"] == "done" and events[-1]["observations"] == 10


def test_image_sequence_reads_seqinfo(tmp_path, monkeypatch):
    sequence = tmp_path / "seq"
    (sequence / "img1").mkdir(parents=True)
    (sequence / "seqinfo.ini").write_text(
        "[Sequence]\nname=seq-x\nimDir=img1\nimExt=.png\nseqLength=3\n", encoding="utf-8")
    for index in range(1, 4):
        cv2.imwrite(str(sequence / "img1" / f"{index:06d}.png"), np.zeros((20, 30, 3), np.uint8))
    media = analyze.open_media(sequence)
    assert media.kind == "image-sequence" and media.name == "seq-x" and media.total_frames == 3
    monkeypatch.setattr(analyze, "build_tracker", lambda args, device: _StubTracker())
    output = tmp_path / "out.json"
    assert analyze.main(_args(sequence, output)) == 0
    rows = json.loads(output.read_text(encoding="utf-8"))["observations"]
    assert [row[0] for row in rows] == [1, 2, 3]
    assert all(row[6] <= 29 and row[7] <= 19 for row in rows), "boxes are clipped to the frame"


def test_missing_clip_is_reported_as_an_error_event(tmp_path, capsys):
    folder = tmp_path / "Broken"
    folder.mkdir()
    (folder / "sequence.json").write_text(json.dumps({
        "format": "aquatrack-video-sequence", "clips": [{"path": "gone.mp4", "frames": 3}],
    }), encoding="utf-8")
    assert analyze.main(_args(folder, tmp_path / "out.json")) == 1
    last = json.loads(capsys.readouterr().out.strip().splitlines()[-1])
    assert last["event"] == "error" and "missing" in last["message"]


@pytest.mark.parametrize("name, expected", [("object", "Fish"), ("fish", "Fish"), ("0", "Fish"),
                                            ("crab", "Crab"), ("Small fish", "Small fish")])
def test_display_class_names(name, expected):
    assert analyze.display_class_name(name) == expected
