"""Clear stale code-cell outputs from one or more notebooks.

This is intentionally dependency-free so QC cleanup does not require nbformat.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("notebooks", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.notebooks:
        notebook = json.loads(path.read_text(encoding="utf-8"))
        for cell in notebook.get("cells", []):
            if cell.get("cell_type") == "code":
                cell["execution_count"] = None
                cell["outputs"] = []
                cell.get("metadata", {}).pop("execution", None)
        path.write_text(json.dumps(notebook, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        print(path)


if __name__ == "__main__":
    main()
