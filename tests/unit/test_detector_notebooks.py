import json
from pathlib import Path

import pytest

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize(
    "notebook_name",
    [
        "05_yolo_fish_detection.ipynb",
        "05b_rtdetr_fish_detection.ipynb",
        "05c_faster_rcnn_fish_detection.ipynb",
        "05d_detector_comparison.ipynb",
    ],
)
def test_detector_notebook_code_cells_compile(notebook_name):
    path = REPOSITORY_ROOT / "jupyter_notebook_projects" / notebook_name
    notebook = json.loads(path.read_text(encoding="utf-8"))

    for cell in notebook["cells"]:
        if cell["cell_type"] != "code":
            continue
        source = "".join(cell["source"]) if isinstance(cell["source"], list) else cell["source"]
        compile(source, f"{notebook_name}:{cell.get('id', 'cell')}", "exec")
