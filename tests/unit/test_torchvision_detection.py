import pytest
import torch
from PIL import Image

from aquatrack.torchvision_detection import (
    YoloFormatDetectionDataset,
    ensure_finite_loss,
    yolo_to_xyxy,
)


def test_yolo_to_xyxy_clamps_edge_rounding_to_image_bounds():
    # Rounded YOLO labels can put an edge a few millionths of a pixel outside.
    box = yolo_to_xyxy(0.79088542, 0.01944444, 0.04843750, 0.03888889, 1920, 1080)

    assert box[1] == 0.0
    assert 0.0 <= box[0] < box[2] <= 1920.0
    assert 0.0 <= box[1] < box[3] <= 1080.0


def test_ensure_finite_loss_accepts_finite_tensor():
    ensure_finite_loss(torch.tensor(1.25), context="test")


@pytest.mark.parametrize("value", [float("nan"), float("inf"), -float("inf")])
def test_ensure_finite_loss_rejects_nonfinite_tensor(value):
    with pytest.raises(FloatingPointError, match="Non-finite loss.*test"):
        ensure_finite_loss(torch.tensor(value), context="test")


def test_dataset_rejects_fractional_class_id(tmp_path):
    images = tmp_path / "images"
    labels = tmp_path / "labels"
    images.mkdir()
    labels.mkdir()
    Image.new("RGB", (32, 32)).save(images / "sample.jpg")
    (labels / "sample.txt").write_text("0.5 0.5 0.5 0.25 0.25\n", encoding="utf-8")

    dataset = YoloFormatDetectionDataset(images, labels)
    with pytest.raises(ValueError, match="Invalid YOLO class id"):
        dataset[0]
