from __future__ import annotations

import kornia
import numpy as np
import torch


def get_device() -> torch.device:
    return torch.device("cuda" if torch.cuda.is_available() else "cpu")


def frames_to_batch(frames: list[np.ndarray], device: torch.device) -> torch.Tensor:
    """BGR uint8 (H, W, 3) frames -> one (N, 3, H, W) float32 RGB tensor in
    [0, 1] on `device`. Batching (not one frame at a time) is what actually
    makes GPU preprocessing worth it — a single frame's host<->device
    transfer overhead can dominate over just running it on CPU."""
    stacked = np.stack(frames, axis=0)[..., ::-1]  # (N, H, W, 3) BGR -> RGB
    tensor = torch.from_numpy(np.ascontiguousarray(stacked)).to(device=device, dtype=torch.float32) / 255.0
    return tensor.permute(0, 3, 1, 2).contiguous()  # (N, 3, H, W)


def batch_to_frames(batch: torch.Tensor) -> list[np.ndarray]:
    """(N, 3, H, W) float32 [0, 1] RGB -> list of BGR uint8 (H, W, 3) frames."""
    array = (batch.clamp(0, 1).permute(0, 2, 3, 1) * 255.0).round().to(torch.uint8).cpu().numpy()
    return [frame[..., ::-1].copy() for frame in array]  # RGB -> BGR


def normalize_dynamic_range_gpu(
    batch: torch.Tensor, low_percentile: float = 1.0, high_percentile: float = 99.0,
) -> torch.Tensor:
    """GPU/batched equivalent of preprocessing.normalize_dynamic_range."""
    flat = batch.reshape(batch.shape[0], batch.shape[1], -1)
    low = torch.quantile(flat, low_percentile / 100.0, dim=2).unsqueeze(-1).unsqueeze(-1)
    high = torch.quantile(flat, high_percentile / 100.0, dim=2).unsqueeze(-1).unsqueeze(-1)
    span = (high - low).clamp_min(1e-6)
    return ((batch - low) / span).clamp(0, 1)


def gray_world_white_balance_gpu(batch: torch.Tensor) -> torch.Tensor:
    """GPU/batched equivalent of preprocessing.gray_world_white_balance."""
    channel_means = batch.mean(dim=(2, 3), keepdim=True)  # (N, 3, 1, 1)
    gray_mean = channel_means.mean(dim=1, keepdim=True)
    scale = gray_mean / channel_means.clamp_min(1e-6)
    return (batch * scale).clamp(0, 1)


def clahe_enhance_gpu(
    batch: torch.Tensor, clip_limit: float = 2.0, grid_size: tuple[int, int] = (8, 8),
) -> torch.Tensor:
    """GPU/batched equivalent of preprocessing.clahe_enhance: CLAHE on the L
    channel only (kornia's LAB conversion), same as the CPU path's rationale
    — local contrast without amplifying colour-channel noise.

    clip_limit default matches preprocessing.clahe_enhance's (2.0), NOT
    kornia's own default (40.0): kornia's docs state its CLAHE "uses the same
    approach as in OpenCV", i.e. the same clip_limit scale, where 2-4 is the
    normal range and 40 is close to disabling clipping entirely (near-full
    histogram equalization). Verified empirically — using kornia's own
    default produced a ~2x brightness blowout relative to the CPU path on
    identical input; clip_limit=2.0 matches it within rounding."""
    lab = kornia.color.rgb_to_lab(batch)
    lightness = (lab[:, 0:1] / 100.0).clamp(0, 1)
    lightness_eq = kornia.enhance.equalize_clahe(lightness, clip_limit=clip_limit, grid_size=grid_size)
    lab_eq = torch.cat([lightness_eq * 100.0, lab[:, 1:]], dim=1)
    return kornia.color.lab_to_rgb(lab_eq).clamp(0, 1)


def preprocess_batch_gpu(
    frames: list[np.ndarray],
    normalize_range: bool = True,
    white_balance: bool = True,
    clahe: bool = True,
    clip_limit: float = 2.0,
    grid_size: tuple[int, int] = (8, 8),
    device: torch.device | None = None,
) -> list[np.ndarray]:
    """Batched GPU equivalent of preprocessing.preprocess_frame — same three
    stages, same order, run on the whole list of frames at once as a single
    tensor instead of a Python loop calling OpenCV per frame."""
    device = device or get_device()
    batch = frames_to_batch(frames, device)
    if normalize_range:
        batch = normalize_dynamic_range_gpu(batch)
    if white_balance:
        batch = gray_world_white_balance_gpu(batch)
    if clahe:
        batch = clahe_enhance_gpu(batch, clip_limit=clip_limit, grid_size=grid_size)
    return batch_to_frames(batch)
