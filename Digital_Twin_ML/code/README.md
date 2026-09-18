# Machine learning pipeline code

Machine learning pipeline for the C++ methanol plant digital twin. The `common/` package provides shared utilities, while `surrogate/` and `feasibility/` contain model architectures. Execute commands from this `code/` directory.

## Directory layout

code/
  common/             shared foundation
    design_space.py   parameter definitions and sampling bounds
    twin_interface.py bridge to compiled twin and offline MockTwin
    sampling.py       Latin hypercube and Sobol sampling plans
    schema.py         dataset row layout and file serialisation
    provenance.py     fingerprint verification and contract hashing
    generate_dataset.py sampling, evaluation, labeling, and storage
    plot_style.py     figure styles, palettes, and physical units
  surrogate/          scalar surrogate regression
    train.py          gradient boosted trees and gaussian process uncertainty
    plots.py          parity and relative importance figures
    sensitivity.py    first-order and total-effect Sobol indices
    serving.py        provenance validation and out-of-distribution guards
  feasibility/        feasibility and failure classification
    train.py          calibrated binary classifier and multiclass failure head
    plots.py          ROC, PR, and outcome confusion matrices
  run_all.py          executes surrogate, feasibility, and figure generation
  optimize.py         surrogate-assisted design optimization
  explore.py          response curves, Sobol bars, Pareto frontier, surfaces
  smoke_test.py       fast verification test on mock twin

## Dependencies

Dependencies are managed in `pyproject.toml` at the project root (`Digital_Twin_ML/pyproject.toml`).

## Pipeline execution

### 1. End-to-end execution

```powershell
python run_all.py
```

Writes datasets to `../data/`, models to `../models/`, and diagnostic figures to `../figures/`.

### 2. Individual model execution

If `../data/pilot.parquet` is present, scripts reuse the dataset:

Surrogate model:
```powershell
python -m surrogate.train
python -m surrogate.plots
```

Feasibility classifier:
```powershell
python -m feasibility.train
python -m feasibility.plots
```

### 3. Physical twin backend

To evaluate against the compiled C++ module:
```powershell
python run_all.py --backend pybind --module digital_twin
```
