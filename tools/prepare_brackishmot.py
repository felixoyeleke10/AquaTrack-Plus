from __future__ import annotations

import argparse
import json
from pathlib import Path

from aquatrack.datasets.brackishmot import prepare_dataset


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert BrackishMOT frames and MOT annotations into a YOLO dataset."
    )
    parser.add_argument("--source", type=Path, required=True, help="Extracted BrackishMOT root")
    parser.add_argument("--output", type=Path, default=Path("data/brackishmot"))
    parser.add_argument("--validation-fraction", type=float, default=0.20)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument(
        "--method", choices=["hardlink", "copy"], default="hardlink",
        help="Hard links save disk space and automatically fall back to copying.",
    )
    parser.add_argument(
        "--fish-only", action="store_true",
        help="Merge fish and small-fish into one class; ignore other animal labels.",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()
    report = prepare_dataset(
        source=args.source,
        output=args.output,
        validation_fraction=args.validation_fraction,
        seed=args.seed,
        method=args.method,
        fish_only=args.fish_only,
    )
    print(json.dumps(report, indent=2))
    print(f"\nDataset YAML: {(args.output.resolve() / 'dataset.yaml')}")


if __name__ == "__main__":
    main()

