# Methanol twin ML environment

`pyproject.toml` declares the ML dependencies and uses the local
`../python/digital_twin` package. `uv.lock` records the resolved versions.
This is an experiment environment (`package = false`), not a separately
published Python package.

From this directory:

```powershell
uv sync
uv run python code/transformers/scripts/generate_dataset.py
uv run python code/transformers/scripts/train_baseline.py
```

The dataset script writes `design_points.csv` in the current directory; run
both scripts from the same directory. The first command builds the C++
pybind11 extension as a local dependency. In VS Code, select
`Digital_Twin_ML/.venv/Scripts/python.exe` as the Python interpreter.

Generated data, caches, and the virtual environment are ignored by Git.
The lockfile and project metadata are intended to be committed.
