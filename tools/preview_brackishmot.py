from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path

import cv2

from aquatrack.datasets.brackishmot import (
    BRACKISHMOT_CLASSES,
    parse_annotations,
    read_sequence_info,
)


COLORS = {
    1: (55, 210, 80),
    2: (255, 170, 50),
    3: (180, 100, 255),
    4: (60, 200, 240),
    5: (255, 100, 80),
    6: (230, 100, 210),
}


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Render BrackishMOT ground truth as MP4.")
    parser.add_argument("--sequence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fish-only", action="store_true")
    parser.add_argument("--max-frames", type=int, default=0, help="0 renders the full sequence")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    info = read_sequence_info(args.sequence)
    by_frame = defaultdict(list)
    for annotation in parse_annotations(args.sequence):
        by_frame[annotation.frame].append(annotation)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(
        str(args.output), cv2.VideoWriter_fourcc(*"mp4v"),
        info.frame_rate, (info.width, info.height),
    )
    if not writer.isOpened():
        raise RuntimeError(f"Could not create preview: {args.output}")
    rendered = 0
    try:
        for image_path in sorted(info.image_dir.glob(f"*{info.extension}")):
            frame_index = int(image_path.stem)
            frame = cv2.imread(str(image_path))
            if frame is None:
                raise RuntimeError(f"Could not read frame: {image_path}")
            for annotation in by_frame.get(frame_index, []):
                if args.fish_only and annotation.class_id not in {1, 5}:
                    continue
                x1, y1 = int(annotation.left), int(annotation.top)
                x2 = int(annotation.left + annotation.width)
                y2 = int(annotation.top + annotation.height)
                color = COLORS[annotation.class_id]
                label = f"{BRACKISHMOT_CLASSES[annotation.class_id]} #{annotation.track_id}"
                cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                cv2.putText(
                    frame, label, (x1, max(18, y1 - 6)), cv2.FONT_HERSHEY_SIMPLEX,
                    0.5, color, 2, cv2.LINE_AA,
                )
            writer.write(frame)
            rendered += 1
            if args.max_frames and rendered >= args.max_frames:
                break
    finally:
        writer.release()
    print(f"Rendered {rendered} frames to {args.output.resolve()}")


if __name__ == "__main__":
    main()

