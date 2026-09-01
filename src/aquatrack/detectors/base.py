from __future__ import annotations

from typing import Protocol

import numpy as np

from aquatrack.types import Detection


class Detector(Protocol):
    def detect(self, frame: np.ndarray) -> list[Detection]: ...

