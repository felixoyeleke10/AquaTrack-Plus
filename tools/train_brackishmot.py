from __future__ import annotations

import argparse
from pathlib import Path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Train a YOLO detector on prepared BrackishMOT data.")
    parser.add_argument("--data", type=Path, default=Path("data/brackishmot/dataset.yaml"))
    parser.add_argument("--model", default="yolo11n.pt")
    parser.add_argument("--epochs", type=int, default=75)
    parser.add_argument("--image-size", type=int, default=960)
    parser.add_argument("--batch", type=int, default=8)
    parser.add_argument("--device", default=None, help="Examples: 0, cpu, 0,1")
    parser.add_argument("--project", type=Path, default=Path("runs/brackishmot"))
    parser.add_argument("--name", default="yolo11n")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    if not args.data.is_file():
        raise FileNotFoundError(f"Prepared dataset YAML not found: {args.data}")
    from ultralytics import YOLO

    model = YOLO(args.model)
    model.train(
        data=str(args.data.resolve()),
        epochs=args.epochs,
        imgsz=args.image_size,
        batch=args.batch,
        device=args.device,
        project=str(args.project),
        name=args.name,
    )


if __name__ == "__main__":
    main()

