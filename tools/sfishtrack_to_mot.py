"""Convert SFISHTRACK tracking annotations to MOTChallenge gt.txt aligned to the MP4.

SFISHTRACK annotates frames extracted from each video (``Frames/video_NNN``),
and those extracted frames are a *resampled* copy of ``Videos/video_NNN.mp4``:
for many videos the MP4 has more frames, and some extractions start seconds
into the video. Annotation frame k is therefore not MP4 frame k, and boxes
placed by number drift off the fish.

This tool finds, for every annotated image, the MP4 frame with the same
pixels (global search on anchor frames, a robust linear fit, then a local
search that keeps the order increasing), and writes the boxes at those MP4
frame numbers. It refuses to write a file when the images do not match the
video, rather than produce plausible-looking but wrong ground truth.

    python tools/sfishtrack_to_mot.py --root C:/data/SFISHTRACK/SFISHTRACK --video 45 \
        --output gt/video_045.gt.txt

The output imports into AquaTrack+ with Review -> Import annotations, or
``aquatrack-cli project annotations``.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

import cv2
import numpy as np

THUMB = (96, 54)


def thumbnail(image: np.ndarray) -> np.ndarray:
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY) if image.ndim == 3 else image
    return cv2.resize(gray, THUMB, interpolation=cv2.INTER_AREA).astype(np.float32)


def video_thumbnails(path: Path) -> np.ndarray:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise SystemExit(f"Could not open {path}")
    thumbs = []
    while True:
        ok, frame = capture.read()
        if not ok:
            break
        thumbs.append(thumbnail(frame))
    capture.release()
    return np.stack(thumbs)


def difference(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    return np.abs(b - a).mean(axis=tuple(range(1, b.ndim))) if b.ndim == 3 else np.abs(b - a).mean()


def align(annotated: list[np.ndarray], video: np.ndarray) -> tuple[list[int], list[float], tuple[float, float]]:
    """MP4 index for each annotated image, its pixel difference, and the fit."""
    count = len(annotated)
    anchors = sorted(set(np.linspace(0, count - 1, min(count, 25)).astype(int).tolist()))
    pairs = []
    for k in anchors:
        scores = difference(annotated[k], video)
        pairs.append((k, int(np.argmin(scores)), float(scores.min())))
    ks = np.array([p[0] for p in pairs], dtype=float)
    vs = np.array([p[1] for p in pairs], dtype=float)
    slope, intercept = np.polyfit(ks, vs, 1)
    # Drop anchors far off the line (look-alike frames) and refit.
    residual = np.abs(vs - (slope * ks + intercept))
    keep = residual <= max(2.0, 3 * float(np.median(residual)))
    if keep.sum() >= 2:
        slope, intercept = np.polyfit(ks[keep], vs[keep], 1)

    # Order-preserving alignment with the lowest total pixel difference
    # (dynamic programming over a band around the fitted line). Choosing
    # each frame greedily lets one look-alike match push every later frame
    # off by one.
    band = 8
    candidates = []
    costs = []
    for k, image in enumerate(annotated):
        predicted = int(round(slope * k + intercept))
        low = max(0, predicted - band)
        high = min(len(video) - 1, predicted + band)
        options = list(range(low, high + 1)) or [min(len(video) - 1, max(0, predicted))]
        candidates.append(options)
        costs.append(difference(image, video[options[0]:options[-1] + 1]))

    total = [costs[0].astype(float)]
    back: list[np.ndarray] = [np.full(len(candidates[0]), -1)]
    for k in range(1, len(annotated)):
        previous_options = np.array(candidates[k - 1])
        previous_total = total[-1]
        current = np.full(len(candidates[k]), np.inf)
        pointer = np.full(len(candidates[k]), -1)
        for position, frame in enumerate(candidates[k]):
            allowed = previous_options < frame
            if not allowed.any():
                continue
            choice = int(np.argmin(np.where(allowed, previous_total, np.inf)))
            if np.isfinite(previous_total[choice]):
                current[position] = previous_total[choice] + costs[k][position]
                pointer[position] = choice
        total.append(current)
        back.append(pointer)

    indices = [-1] * len(annotated)
    scores_out = [float("inf")] * len(annotated)
    position = int(np.argmin(total[-1]))
    if not np.isfinite(total[-1][position]):
        return indices, scores_out, (float(slope), float(intercept))
    for k in range(len(annotated) - 1, -1, -1):
        indices[k] = candidates[k][position]
        scores_out[k] = float(costs[k][position])
        position = int(back[k][position])
    return indices, scores_out, (float(slope), float(intercept))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--root", type=Path, required=True, help="SFISHTRACK folder (Videos/, Frames/, Annotations/)")
    parser.add_argument("--video", type=int, required=True, help="Video number, e.g. 45")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-difference", type=float, default=6.0,
                        help="Largest mean grey-level difference accepted for a match")
    args = parser.parse_args(argv)

    number = f"{args.video:03d}"
    video_path = args.root / "Videos" / f"video_{number}.mp4"
    annotation_path = args.root / "Annotations" / f"video{number}.json"
    frames_dir = args.root / "Frames" / f"video_{number}"
    data = json.loads(annotation_path.read_text(encoding="utf-8"))
    images = sorted(data["images"], key=lambda image: image.get("frame_id", 0))

    annotated = []
    for image in images:
        path = frames_dir / image["file_name"]
        picture = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
        if picture is None:
            print(f"error: annotated image {path} is missing; cannot align by content", file=sys.stderr)
            return 2
        annotated.append(thumbnail(picture))

    video = video_thumbnails(video_path)
    indices, differences, (slope, intercept) = align(annotated, video)
    finite = [d for d in differences if np.isfinite(d)]
    median = float(np.median(finite)) if finite else float("inf")
    worst = float(max(finite)) if finite else float("inf")
    if median > args.max_difference or any(i < 0 for i in indices):
        print(f"error: annotated frames do not match {video_path.name} (median difference "
              f"{median:.2f}); not writing ground truth", file=sys.stderr)
        return 3

    frame_of_image = {image["id"]: indices[position] + 1 for position, image in enumerate(images)}
    rows = defaultdict(list)
    for annotation in data["annotations"]:
        frame = frame_of_image.get(annotation["image_id"])
        track = annotation.get("track_id", annotation.get("instance_id"))
        if frame is None or track is None:
            continue
        x, y, w, h = annotation["bbox"]
        # Class 1 is Fish in the BrackishMOT numbering AquaTrack+ uses.
        rows[frame].append(f"{frame},{int(track)},{x:.1f},{y:.1f},{w:.1f},{h:.1f},1,1,1")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as handle:
        for frame in sorted(rows):
            handle.write("\n".join(rows[frame]) + "\n")

    boxes = sum(len(value) for value in rows.values())
    print(json.dumps({
        "video": video_path.name, "videoFrames": int(len(video)), "annotatedFrames": len(images),
        "firstFrame": indices[0] + 1, "lastFrame": indices[-1] + 1,
        "mapping": f"mp4 = {slope:.4f} * k + {intercept:.1f}",
        "medianDifference": round(median, 2), "worstDifference": round(worst, 2),
        "boxes": boxes, "output": str(args.output),
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
