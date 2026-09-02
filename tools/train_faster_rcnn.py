"""Unattended, fail-fast Faster R-CNN benchmark training.

This runner exists separately from the exploratory notebook so a long job never
overwrites notebook source via ``nbconvert --inplace``. It uses the same split,
640-pixel transform, and benchmark metric schema as notebooks 05c/05d.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import torch
import yaml
from torch.utils.data import DataLoader
from torchmetrics.detection import MeanAveragePrecision
from torchvision.models.detection import (
    FasterRCNN_ResNet50_FPN_V2_Weights,
    fasterrcnn_resnet50_fpn_v2,
)
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "src"))

from aquatrack.benchmark import count_matches, save_detector_metrics  # noqa: E402
from aquatrack.torchvision_detection import (  # noqa: E402
    YoloFormatDetectionDataset,
    collate_fn,
    ensure_finite_loss,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dataset-yaml",
        type=Path,
        default=Path(r"C:\Users\oyele\AquaTrack_data\sfishtrack_yolo\dataset.yaml"),
    )
    parser.add_argument("--max-epochs", type=int, default=50)
    parser.add_argument("--patience", type=int, default=5)
    parser.add_argument("--max-hours", type=float, default=3.5)
    parser.add_argument("--batch-size", type=int, default=8)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--run-name", default="faster_rcnn_converged")
    parser.add_argument(
        "--project", type=Path, default=REPOSITORY_ROOT / "runs" / "fish_detector_benchmark"
    )
    return parser.parse_args()


def make_model(num_classes: int, device: torch.device):
    model = fasterrcnn_resnet50_fpn_v2(
        weights=FasterRCNN_ResNet50_FPN_V2_Weights.DEFAULT,
        min_size=640,
        max_size=640,
    )
    features = model.roi_heads.box_predictor.cls_score.in_features
    model.roi_heads.box_predictor = FastRCNNPredictor(features, num_classes)
    return model.to(device)


@torch.no_grad()
def evaluate(model, loader, device: torch.device) -> tuple[float, float, float, float]:
    model.eval()
    metric = MeanAveragePrecision(box_format="xyxy", iou_type="bbox")
    tp = fp = fn = 0
    for images, targets in loader:
        predictions = model([image.to(device, non_blocking=True) for image in images])
        predictions_cpu = [{key: value.cpu() for key, value in item.items()} for item in predictions]
        metric.update(predictions_cpu, targets)
        for prediction, target in zip(predictions_cpu, targets):
            batch_tp, batch_fp, batch_fn = count_matches(
                prediction["boxes"],
                prediction["scores"],
                target["boxes"],
                score_threshold=0.5,
                iou_threshold=0.5,
            )
            tp += batch_tp
            fp += batch_fp
            fn += batch_fn
    result = metric.compute()
    precision = tp / (tp + fp) if tp + fp else 0.0
    recall = tp / (tp + fn) if tp + fn else 0.0
    return precision, recall, float(result["map_50"]), float(result["map"])


def main() -> None:
    args = parse_args()
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA is required for the full Faster R-CNN benchmark run")
    torch.manual_seed(42)
    device = torch.device("cuda:0")

    config = yaml.safe_load(args.dataset_yaml.read_text(encoding="utf-8"))
    dataset_root = Path(config["path"])
    train_dataset = YoloFormatDetectionDataset(
        dataset_root / "images" / "train", dataset_root / "labels" / "train"
    )
    val_dataset = YoloFormatDetectionDataset(
        dataset_root / "images" / "val", dataset_root / "labels" / "val"
    )
    loader_options = {
        "batch_size": args.batch_size,
        "collate_fn": collate_fn,
        "num_workers": args.workers,
        "pin_memory": True,
        "persistent_workers": args.workers > 0,
    }
    train_loader = DataLoader(train_dataset, shuffle=True, **loader_options)
    val_loader = DataLoader(val_dataset, shuffle=False, **loader_options)

    model = make_model(len(config["names"]) + 1, device)
    optimizer = torch.optim.SGD(
        [parameter for parameter in model.parameters() if parameter.requires_grad],
        lr=0.005,
        momentum=0.9,
        weight_decay=0.0005,
    )
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, mode="max", factor=0.1, patience=1, min_lr=1e-6
    )
    run_dir = args.project / args.run_name
    weights_dir = run_dir / "weights"
    weights_dir.mkdir(parents=True, exist_ok=True)

    training_started = time.perf_counter()
    best_map50_95 = -1.0
    best_metrics = None
    best_epoch = None
    stale_epochs = 0
    completed_epochs = 0
    for epoch in range(1, args.max_epochs + 1):
        model.train()
        started = time.perf_counter()
        running_loss = 0.0
        for batch_index, (images, targets) in enumerate(train_loader, 1):
            images = [image.to(device, non_blocking=True) for image in images]
            targets = [
                {key: value.to(device, non_blocking=True) for key, value in target.items()}
                for target in targets
            ]
            loss_parts = model(images, targets)
            loss = sum(loss_parts.values())
            ensure_finite_loss(loss, context=f"Faster R-CNN epoch {epoch}, batch {batch_index}")
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()

        precision, recall, map50, map50_95 = evaluate(model, val_loader, device)
        scheduler.step(map50_95)
        completed_epochs = epoch
        checkpoint = {
            "epoch": epoch,
            "model": model.state_dict(),
            "optimizer": optimizer.state_dict(),
            "scheduler": scheduler.state_dict(),
            "mean_loss": running_loss / len(train_loader),
        }
        torch.save(checkpoint, weights_dir / "last_checkpoint.pt")
        torch.save(model.state_dict(), weights_dir / f"epoch{epoch}.pt")
        if map50_95 > best_map50_95:
            best_map50_95 = map50_95
            best_metrics = (precision, recall, map50, map50_95)
            best_epoch = epoch
            stale_epochs = 0
            torch.save(model.state_dict(), weights_dir / "best.pt")
        else:
            stale_epochs += 1
        print(
            f"epoch {epoch}/{args.max_epochs}: loss={checkpoint['mean_loss']:.6f}, "
            f"map50_95={map50_95:.6f}, best={best_map50_95:.6f}, stale={stale_epochs}, "
            f"lr={optimizer.param_groups[0]['lr']:.2e}, seconds={time.perf_counter() - started:.1f}",
            flush=True,
        )
        if stale_epochs >= args.patience:
            print(f"early stopping after {stale_epochs} epochs without mAP50-95 improvement", flush=True)
            break
        if (time.perf_counter() - training_started) / 3600 >= args.max_hours:
            print(f"stopping at the {args.max_hours:.2f}-hour wall-clock limit", flush=True)
            break

    if best_metrics is None:
        raise RuntimeError("Training ended without a valid evaluation")
    precision, recall, map50, map50_95 = best_metrics
    best_weights = weights_dir / "best.pt"
    metrics_path = save_detector_metrics(
        args.project / "metrics",
        args.run_name,
        model="Faster R-CNN (ResNet50-FPN-v2)",
        framework="torchvision_fasterrcnn",
        weights_path=str(best_weights),
        epochs=completed_epochs,
        best_epoch=best_epoch,
        metric_protocol="torchmetrics_coco_map_and_fixed_confidence_0.5_pr",
        pilot=False,
        precision=precision,
        recall=recall,
        map50=map50,
        map50_95=map50_95,
    )
    print(f"metrics={metrics_path}", flush=True)


if __name__ == "__main__":
    main()
