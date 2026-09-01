from __future__ import annotations

import argparse
import json
from dataclasses import asdict
from pathlib import Path

from aquatrack.config import load_config
from aquatrack.detectors import YoloDetector
from aquatrack.pipeline import process_video


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="aquatrack", description="Detect, track, and count fish in underwater video."
    )
    parser.add_argument("video", type=Path, help="Input video path")
    parser.add_argument("--output", type=Path, default=Path("outputs"))
    parser.add_argument("--config", type=Path, default=Path("configs/default.yaml"))
    parser.add_argument("--weights", type=str, help="Override YOLO weights")
    parser.add_argument("--confidence", type=float, help="Override detection confidence")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    config = load_config(args.config)
    if args.weights:
        config.model.weights = args.weights
    if args.confidence is not None:
        config.model.confidence = args.confidence
    detector = YoloDetector(config.model)
    result = process_video(args.video, args.output, detector, config)
    payload = asdict(result)
    payload = {key: str(value) if isinstance(value, Path) else value for key, value in payload.items()}
    print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()

