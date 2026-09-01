from pathlib import Path

import pytest

from aquatrack.config import load_config


def test_default_config_loads() -> None:
    config = load_config(Path("configs/default.yaml"))
    assert config.model.weights == "yolo11n.pt"
    assert config.tracking.min_confirmed_hits == 3


def test_invalid_line_position_is_rejected(tmp_path: Path) -> None:
    path = tmp_path / "bad.yaml"
    path.write_text("counting:\n  line_position: 2\n", encoding="utf-8")
    with pytest.raises(ValueError, match="line_position"):
        load_config(path)

