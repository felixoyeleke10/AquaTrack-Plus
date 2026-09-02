from __future__ import annotations

import cv2
import numpy as np


def normalize_dynamic_range(
    frame: np.ndarray, low_percentile: float = 1.0, high_percentile: float = 99.0,
) -> np.ndarray:
    """Per-channel percentile stretch to fill the full 0-255 range.

    Necessary (not just cosmetic) for footage like the extended-gamut/log-style
    clips found in 02_video_exploration: real content can occupy only a narrow
    low band of the 8-bit range (e.g. 0-6 out of 255), and CLAHE's local
    equalization alone doesn't reliably rescue that — this establishes a
    sane global range first.
    """
    result = frame.astype(np.float64)
    for channel in range(3):
        plane = result[:, :, channel]
        low = np.percentile(plane, low_percentile)
        high = np.percentile(plane, high_percentile)
        if high <= low:
            continue
        result[:, :, channel] = np.clip((plane - low) / (high - low) * 255.0, 0, 255)
    return result.astype(np.uint8)


def gray_world_white_balance(frame: np.ndarray) -> np.ndarray:
    """Scales each channel so its mean matches the overall gray mean.

    Underwater footage loses red first with depth/distance (see
    frame_quality_metrics' red_deficiency in aquatrack.video) — this is the
    standard, cheap correction for that channel imbalance. Not physically
    accurate (a real fix needs depth/distance, which this doesn't have), but
    a solid default before anything more sophisticated.
    """
    result = frame.astype(np.float64)
    channel_means = [result[:, :, i].mean() for i in range(3)]
    gray_mean = sum(channel_means) / 3.0
    for channel, mean in enumerate(channel_means):
        scale = gray_mean / max(mean, 1e-6)
        result[:, :, channel] = np.clip(result[:, :, channel] * scale, 0, 255)
    return result.astype(np.uint8)


def clahe_enhance(
    frame: np.ndarray, clip_limit: float = 2.0, tile_grid_size: tuple[int, int] = (8, 8),
) -> np.ndarray:
    """Contrast-limited adaptive histogram equalization on the L channel only
    (LAB colour space) — boosts local contrast without amplifying the
    colour-channel noise that equalizing B/G/R independently would."""
    lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
    l_channel, a_channel, b_channel = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=clip_limit, tileGridSize=tile_grid_size)
    l_equalized = clahe.apply(l_channel)
    return cv2.cvtColor(cv2.merge((l_equalized, a_channel, b_channel)), cv2.COLOR_LAB2BGR)


def preprocess_frame(
    frame: np.ndarray,
    normalize_range: bool = True,
    white_balance: bool = True,
    clahe: bool = True,
    clip_limit: float = 2.0,
    tile_grid_size: tuple[int, int] = (8, 8),
) -> np.ndarray:
    """The standard underwater preprocessing chain, each stage independently
    toggleable so a notebook can isolate which stage is doing the work.
    Order matters: range normalization first (fix the raw scale), then white
    balance (fix channel ratios), then CLAHE (local contrast) — each stage
    assumes the previous one already ran.

    This is deliberately *not* dehazing (no depth/transmission estimate, e.g.
    Dark Channel Prior) — that's a heavier, model-based step earmarked as a
    future addition here once there's a concrete case it's needed for.
    """
    result = frame
    if normalize_range:
        result = normalize_dynamic_range(result)
    if white_balance:
        result = gray_world_white_balance(result)
    if clahe:
        result = clahe_enhance(result, clip_limit=clip_limit, tile_grid_size=tile_grid_size)
    return result
