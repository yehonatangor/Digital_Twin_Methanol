# Instructions: Methanol digital twin machine learning pipeline

This guide outlines the commands and procedures to evaluate, train, and test the surrogate regression and feasibility classification models, with specific commands for targeted figure generation, plot tracking, and operating envelope visualization.

## 1. Physical simulation dataset versus mock dataset

### Dataset characteristics

1. Physical dataset (`data/pilot.parquet`):
The frozen 6000-run dataset evaluated with the compiled C++ twin using pybind across the widened operational parameter space (`pybind_wide`). It contains 5715 feasible design points and 285 physical failure outcomes. All published performance figures, parity plots, Sobol indices, and classification metrics originate from this dataset.

2. Mock baseline dataset (`data/pilot_mock_baseline.parquet`):
A synthetic 3000-run evaluation generated with algebraic approximations. It evaluates in milliseconds and verifies software execution without requiring the compiled C++ twin.

### Regenerating a 6000-run mock dataset

To generate a 6000-point mock dataset that matches the sample size and wide parameter boundaries of the physical simulation run:

```powershell
python -m common.generate_dataset --backend mock --n 6000 --wide --out ../data/pilot_mock_6000.parquet
```

## 2. Directory conventions

Execute all pipeline commands from the `code/` directory:

```powershell
cd C:\Users\Y\Documents\Projects\Digital_Twin_Methanol\Digital_Twin_ML\code
```

Directory references relative to `code/`:
- Datasets: `../data/`
- Serialized models: `../models/`
- Output figures: `../figures/`

## 3. Rapid verification test

Verify pipeline execution using a temporary directory and the mock backend:

```powershell
python smoke_test.py
```

This command performs:
1. Generation of 1200 mock design points.
2. 5-fold cross-validation of the surrogate regression models.
3. Computation of Sobol sensitivity indices.
4. Testing of out-of-distribution detection.
5. Training of the feasibility classifier.
6. Prints `SMOKE OK` upon completion.

## 4. Reproducing models and figures from the frozen physical dataset

The 6000-point physical dataset in `../data/pilot.parquet` allows full reproduction of models and figures without compiling C++ binaries.

### Option A: Complete pipeline in one command

```powershell
python run_all.py --n 6000
```

Trains both models and outputs all figures to `../figures/`.

### Option B: Step-by-step model training

1. Train surrogate regression models:
```powershell
python -m surrogate.train --data ../data/pilot.parquet --out ../models
```
Outputs `../models/surrogate.joblib` and `../models/cv_report.json`.

2. Train feasibility classifier:
```powershell
python -m feasibility.train --data ../data/pilot.parquet --out ../models
```
Outputs `../models/feasibility.joblib`.

## 5. Targeted figure generation and version tracking

All plotting scripts support targeted generation, run tracking tags (such as `1`, `2`, `3`), and the `--wipe` flag to clean previous versions of a specific plot.

### Surrogate figures (parity and feature importance)

Generate all surrogate plots:
```powershell
python -m surrogate.plots --data ../data/pilot.parquet --model ../models/surrogate.joblib --figures ../figures --tag 1 --wipe
```

Generate plots for a single target:
```powershell
# Unit cost
python -m surrogate.plots --target unit_cost_USD_per_t --tag 1 --wipe

# Carbon dioxide conversion
python -m surrogate.plots --target co2_conversion_overall --tag 1 --wipe

# Methanol production rate
python -m surrogate.plots --target meoh_product_kg_s --tag 1 --wipe

# Annual production
python -m surrogate.plots --target annual_production_t --tag 1 --wipe

# Total electric power
python -m surrogate.plots --target total_electricity_MW --tag 1 --wipe

# Residual and error distribution plots
python -m surrogate.plots --plot residuals --tag 1 --wipe
```

### Feasibility figures (ROC, PR, and confusion matrix)

Generate all feasibility plots:
```powershell
python -m feasibility.plots --data ../data/pilot.parquet --model ../models/feasibility.joblib --figures ../figures --tag 1 --wipe
```

Generate specific feasibility plots:
```powershell
# Receiver operating characteristic and precision-recall curves
python -m feasibility.plots --plot roc_pr --tag 1 --wipe

# Multiclass failure-mode confusion matrix
python -m feasibility.plots --plot confusion --tag 1 --wipe
```

### Parameter exploration figures

Generate individual exploration figures:
```powershell
# Response curves for all targets
python explore.py --plot response --tag 1 --wipe

# Response curves for a single target
python explore.py --plot response --target unit_cost_USD_per_t --tag 1 --wipe

# Sobol global sensitivity indices
python explore.py --plot sobol --tag 1 --wipe

# Pareto frontier (unit cost vs annual production)
python explore.py --plot pareto --tag 1 --wipe

# 3D response surface
python explore.py --plot surface3d --tag 1 --wipe

# 2D contour maps with isolines and failure boundaries
python explore.py --plot contour2d --tag 1 --wipe

# Feasibility and failure boundaries in design space
python explore.py --plot feasible_region --tag 1 --wipe

# Operating envelope
python explore.py --plot envelope --tag 1 --wipe
```

### Operating envelope figure

The operating envelope maps carbon yield against recycle compression duty across feed ratios and purge fractions.

```powershell
# Generate from the ML code package with run tracking
python plot_envelope.py --tag 1 --wipe

# Generate from repo root and update docs/figures
cd C:\Users\Y\Documents\Projects\Digital_Twin_Methanol
python tools/plot_operating_envelope.py --wipe
```

Outputs both PNG (`03-operating-envelope.png`) and clean SVG (`03-operating-envelope.svg`).
For complete engineering equations, stoichiometric ceilings, and deactivation tolerance, see `docs/06-operating-envelope.md`.

## 6. Surrogate-assisted design optimization

```powershell
# Interactive objective selection
python optimize.py

# Minimize unit production cost
python optimize.py --objective min_cost

# Maximize annual production
python optimize.py --objective max_production

# Maximize $\mathrm{CO_2}$ conversion
python optimize.py --objective max_conversion

# Maximize methanol mass flow rate
python optimize.py --objective max_methanol
```

## 7. Generating new datasets from scratch

### Synthetic dataset (mock twin)

```powershell
python -m common.generate_dataset --backend mock --n 6000 --wide --out ../data/pilot.parquet
```

### Physical twin dataset (C++ solver)

Requires the compiled `digital_twin` extension:

```powershell
python -m common.generate_dataset --backend pybind --module digital_twin --n 6000 --wide --jobs 8 --out ../data/pilot.parquet
```

## 8. Clean rebuild workflow

To purge generated artifacts and re-evaluate all models:

```powershell
# 1. Remove previous models and figures
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue ..\models\*, ..\figures\*

# 2. Train surrogate models
python -m surrogate.train --data ../data/pilot.parquet --out ../models

# 3. Generate surrogate plots
python -m surrogate.plots --data ../data/pilot.parquet --model ../models/surrogate.joblib --figures ../figures --tag 1 --wipe

# 4. Train feasibility classifier
python -m feasibility.train --data ../data/pilot.parquet --out ../models

# 5. Generate feasibility plots
python -m feasibility.plots --data ../data/pilot.parquet --model ../models/feasibility.joblib --figures ../figures --tag 1 --wipe

# 6. Generate exploration and operating envelope plots
python explore.py --data ../data/pilot.parquet --model ../models/surrogate.joblib --figures ../figures --tag 1 --wipe
python plot_envelope.py --tag 1 --wipe
```
