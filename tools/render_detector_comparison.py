import json
from pathlib import Path

import pandas as pd

ROOT = Path(r"D:\Software_and_Web\softwares\AquaTrack+")
METRICS = ROOT / "runs/fish_detector_benchmark/metrics"
OUTPUT = ROOT / "runs/fish_detector_benchmark"
RUNS = ["sfishtrack_15ep", "rtdetr_l_converged", "faster_rcnn_converged"]


def main() -> None:
    records = []
    for run_name in RUNS:
        record = json.loads((METRICS / f"{run_name}.json").read_text(encoding="utf-8"))
        precision, recall = record["precision"], record["recall"]
        record["f1"] = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
        records.append(record)
    frame = pd.DataFrame(records)[
        ["model", "run_name", "epochs", "precision", "recall", "f1", "map50", "map50_95", "weights_path"]
    ]
    frame.to_csv(OUTPUT / "detector_comparison_final.csv", index=False)

    plot_frame = frame.set_index("model")[["precision", "recall", "f1", "map50", "map50_95"]]
    ax = plot_frame.plot.bar(figsize=(12, 6), width=0.78)
    ax.set_ylim(0, max(0.65, float(plot_frame.max().max()) * 1.15))
    ax.set_ylabel("score")
    ax.set_title("Fish detector comparison — validation-selected checkpoints")
    ax.grid(axis="y", alpha=0.25)
    ax.tick_params(axis="x", rotation=12)
    ax.legend(ncol=5, loc="upper center")
    ax.figure.tight_layout()
    ax.figure.savefig(OUTPUT / "detector_comparison_final.png", dpi=160, bbox_inches="tight")
    print(frame.to_string(index=False))


if __name__ == "__main__":
    main()
