from pathlib import Path

import cv2
import matplotlib.pyplot as plt
import torch
import yaml
from torchvision.models.detection import fasterrcnn_resnet50_fpn_v2
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor


ROOT = Path(r"D:\Software_and_Web\softwares\AquaTrack+")
DATASET_YAML = Path(r"C:\Users\oyele\AquaTrack_data\sfishtrack_yolo\dataset.yaml")
WEIGHTS = ROOT / "runs/fish_detector_benchmark/faster_rcnn_converged/weights/best.pt"
OUTPUT = ROOT / "runs/fish_detector_benchmark/faster_rcnn_converged/inference_grid.jpg"
SCORE_THRESHOLD = 0.5


def main() -> None:
    config = yaml.safe_load(DATASET_YAML.read_text(encoding="utf-8"))
    dataset_root = Path(config["path"])
    if not dataset_root.is_absolute():
        dataset_root = (DATASET_YAML.parent / dataset_root).resolve()
    names = config["names"]
    class_count = len(names) if not isinstance(names, dict) else len(names.keys())

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = fasterrcnn_resnet50_fpn_v2(
        weights=None,
        min_size=640,
        max_size=640,
    )
    model.roi_heads.box_predictor = FastRCNNPredictor(
        model.roi_heads.box_predictor.cls_score.in_features, class_count + 1
    )
    model.load_state_dict(torch.load(WEIGHTS, map_location=device, weights_only=True))
    model.to(device).eval()

    images_dir = dataset_root / "images" / "val"
    labels_dir = dataset_root / "labels" / "val"
    all_images = sorted(images_dir.glob("*.*"))
    samples = all_images[:: max(1, len(all_images) // 6)][:6]
    fig, axes = plt.subplots(2, 3, figsize=(15, 8))
    with torch.no_grad():
        for ax, image_path in zip(axes.flat, samples):
            bgr = cv2.imread(str(image_path))
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
            tensor = torch.from_numpy(rgb).permute(2, 0, 1).float().div(255).to(device)
            prediction = model([tensor])[0]
            keep = prediction["scores"] >= SCORE_THRESHOLD
            boxes = prediction["boxes"][keep].cpu().numpy()
            scores = prediction["scores"][keep].cpu().numpy()
            annotated = rgb.copy()
            for box, score in zip(boxes.astype(int), scores):
                x1, y1, x2, y2 = box
                cv2.rectangle(annotated, (x1, y1), (x2, y2), (0, 255, 0), 3)
                cv2.putText(annotated, f"{score:.2f}", (x1, max(18, y1 - 5)), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 0), 2)
            label_path = labels_dir / f"{image_path.stem}.txt"
            ground_truth = len(label_path.read_text(encoding="utf-8").splitlines()) if label_path.is_file() else 0
            ax.imshow(annotated)
            ax.set_title(f"{image_path.name}\ngt={ground_truth} pred={len(boxes)}", fontsize=9)
            ax.axis("off")
    fig.suptitle("Faster R-CNN best-checkpoint validation inference", fontsize=14)
    fig.tight_layout()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=140, bbox_inches="tight")
    print(OUTPUT)


if __name__ == "__main__":
    main()
