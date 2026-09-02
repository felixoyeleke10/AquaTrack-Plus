from __future__ import annotations

from pathlib import Path
import math

import torch
from PIL import Image
from torch.utils.data import Dataset

from aquatrack.benchmark import count_matches


def yolo_to_xyxy(
    cx: float, cy: float, w: float, h: float, image_width: int, image_height: int
) -> tuple[float, float, float, float]:
    """YOLO's normalized (center_x, center_y, width, height) -> absolute-pixel
    (xmin, ymin, xmax, ymax), the box format torchvision's detection models expect."""
    cx *= image_width
    cy *= image_height
    w *= image_width
    h *= image_height
    # Labels touching an image edge are commonly rounded to eight decimals.
    # Converting those values can produce tiny negatives (for example -5e-6
    # pixels), which violate TorchVision's target contract. Clamp after the
    # conversion while preserving legitimately edge-touching boxes.
    return (
        max(0.0, min(float(image_width), cx - w / 2)),
        max(0.0, min(float(image_height), cy - h / 2)),
        max(0.0, min(float(image_width), cx + w / 2)),
        max(0.0, min(float(image_height), cy + h / 2)),
    )


def ensure_finite_loss(loss: torch.Tensor, *, context: str = "training") -> None:
    """Abort immediately instead of spending hours propagating NaN/Inf."""
    if not bool(torch.isfinite(loss).all()):
        raise FloatingPointError(f"Non-finite loss detected during {context}: {loss.detach().cpu()}")


class YoloFormatDetectionDataset(Dataset):
    """Reads the same flat images/{split}, labels/{split} directories
    04_dataset_preparation.ipynb produces for YOLO, so every detector in the
    benchmark (05, 05b, 05c) trains and validates against literally the same
    files -- no separate copy, no separate split logic to drift out of sync.

    torchvision detection models reserve class index 0 for background, so
    YOLO's class ids are shifted by +1 (YOLO class 0 "fish" -> label 1 here).
    """

    def __init__(self, images_dir: str | Path, labels_dir: str | Path):
        self.images_dir = Path(images_dir)
        self.labels_dir = Path(labels_dir)
        self.image_paths = sorted(
            p for p in self.images_dir.iterdir() if p.suffix.lower() in (".jpg", ".jpeg", ".png")
        )

    def __len__(self) -> int:
        return len(self.image_paths)

    def __getitem__(self, index: int):
        image_path = self.image_paths[index]
        image = Image.open(image_path).convert("RGB")
        width, height = image.size

        label_path = self.labels_dir / f"{image_path.stem}.txt"
        boxes: list[list[float]] = []
        labels: list[int] = []
        if label_path.is_file():
            for line in label_path.read_text(encoding="utf-8").splitlines():
                if not line.strip():
                    continue
                values = [float(v) for v in line.split()]
                if len(values) != 5 or not all(math.isfinite(v) for v in values):
                    raise ValueError(f"Invalid YOLO label at {label_path}: {line!r}")
                class_id, cx, cy, w, h = values
                if class_id < 0 or class_id != int(class_id):
                    raise ValueError(f"Invalid YOLO class id at {label_path}: {class_id}")
                box = yolo_to_xyxy(cx, cy, w, h, width, height)
                if box[2] <= box[0] or box[3] <= box[1]:
                    raise ValueError(f"Degenerate YOLO box at {label_path}: {line!r} -> {box}")
                boxes.append(list(box))
                labels.append(int(class_id) + 1)  # 0 is background in torchvision's convention

        target = {
            "boxes": torch.tensor(boxes, dtype=torch.float32) if boxes else torch.zeros((0, 4), dtype=torch.float32),
            "labels": torch.tensor(labels, dtype=torch.int64) if labels else torch.zeros((0,), dtype=torch.int64),
            "image_id": torch.tensor([index]),
        }

        from torchvision.transforms.functional import to_tensor
        return to_tensor(image), target


def collate_fn(batch):
    return tuple(zip(*batch))


@torch.no_grad()
def evaluate_precision_recall(
    model, data_loader, device, score_threshold: float = 0.5, iou_threshold: float = 0.5
) -> tuple[float, float]:
    """Fixed-operating-point precision/recall (score >= score_threshold, IoU >=
    iou_threshold, greedy matching in descending-confidence order) -- the same
    kind of single-point precision/recall Ultralytics reports at its default
    operating point. torchmetrics' MeanAveragePrecision only gives curves
    averaged over IoU/confidence thresholds, not this directly comparable
    single number, so this fills that gap rather than replacing it."""
    model.eval()
    true_positives = 0
    false_positives = 0
    false_negatives = 0
    for images, targets in data_loader:
        images = [image.to(device) for image in images]
        predictions = model(images)
        for prediction, target in zip(predictions, targets):
            tp, fp, fn = count_matches(
                prediction["boxes"].cpu(), prediction["scores"].cpu(), target["boxes"],
                score_threshold=score_threshold, iou_threshold=iou_threshold,
            )
            true_positives += tp
            false_positives += fp
            false_negatives += fn

    precision = true_positives / (true_positives + false_positives) if (true_positives + false_positives) else 0.0
    recall = true_positives / (true_positives + false_negatives) if (true_positives + false_negatives) else 0.0
    return precision, recall
