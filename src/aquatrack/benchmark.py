from __future__ import annotations

import json
from pathlib import Path

import pandas as pd
import torch
from torchvision.ops import box_iou


def save_detector_metrics(metrics_dir: str | Path, run_name: str, **fields) -> Path:
    """Writes one detector's pilot/final metrics as its own JSON file, so
    05d_detector_comparison.ipynb can assemble the comparison table by
    reading whichever of these files exist rather than each notebook having
    to know about the others."""
    metrics_dir = Path(metrics_dir)
    metrics_dir.mkdir(parents=True, exist_ok=True)
    path = metrics_dir / f"{run_name}.json"
    path.write_text(json.dumps({"run_name": run_name, **fields}, indent=2), encoding="utf-8")
    return path


def load_detector_metrics(metrics_dir: str | Path) -> pd.DataFrame:
    metrics_dir = Path(metrics_dir)
    rows = [
        json.loads(path.read_text(encoding="utf-8"))
        for path in sorted(metrics_dir.glob("*.json"))
    ]
    return pd.DataFrame(rows)


def count_matches(
    pred_boxes: torch.Tensor,
    pred_scores: torch.Tensor,
    gt_boxes: torch.Tensor,
    score_threshold: float = 0.5,
    iou_threshold: float = 0.5,
) -> tuple[int, int, int]:
    """Greedy IoU matching (in descending-confidence order) between one
    image's predicted and ground-truth boxes. Returns (true_positives,
    false_positives, false_negatives) at a single, fixed operating point --
    the same kind of single-number precision/recall Ultralytics reports,
    as opposed to torchmetrics' MeanAveragePrecision, which only gives
    curves averaged over thresholds. Framework-agnostic: takes plain
    tensors, not a specific model's output format, so it works the same
    way for YOLO, RT-DETR, and TorchVision predictions alike."""
    keep = pred_scores >= score_threshold
    pred_boxes = pred_boxes[keep]
    pred_scores = pred_scores[keep]
    order = torch.argsort(pred_scores, descending=True)
    pred_boxes = pred_boxes[order]

    if len(gt_boxes) == 0:
        return 0, len(pred_boxes), 0
    if len(pred_boxes) == 0:
        return 0, 0, len(gt_boxes)

    ious = box_iou(pred_boxes, gt_boxes)
    matched_gt: set[int] = set()
    true_positives = 0
    false_positives = 0
    for pred_index in range(pred_boxes.shape[0]):
        best_iou, best_gt = ious[pred_index].max(dim=0)
        best_gt = int(best_gt)
        if best_iou >= iou_threshold and best_gt not in matched_gt:
            matched_gt.add(best_gt)
            true_positives += 1
        else:
            false_positives += 1
    false_negatives = len(gt_boxes) - len(matched_gt)
    return true_positives, false_positives, false_negatives
