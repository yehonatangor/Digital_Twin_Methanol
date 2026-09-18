"""surrogate training: gradient-boosted models per target plus gaussian process uncertainty layer

    python -m surrogate.train
    python -m surrogate.train --backend pybind --module digital_twin
"""
from __future__ import annotations

import argparse
import json
import os
import sys

import numpy as np
import pandas as pd

from common.generate_dataset import ensure_dataset
from common.twin_interface import TARGETS

# right-skewed targets fit in logarithmic space
LOG_TARGETS = {"unit_cost_USD_per_t", "annual_production_t"}


def _load(path):
    base, ext = os.path.splitext(path)
    df = pd.read_csv(path) if ext == ".csv" else pd.read_parquet(path)
    sidecar = base + ".provenance.json"
    prov = json.load(open(sidecar)).get("provenance") if os.path.exists(sidecar) else None
    return df, prov


def _make_gbm(seed):
    try:
        import lightgbm as lgb
        return "lightgbm", lgb.LGBMRegressor(
            n_estimators=800, learning_rate=0.03, num_leaves=63, subsample=0.8,
            subsample_freq=1, colsample_bytree=0.8, min_child_samples=20,
            reg_lambda=1.0, random_state=seed, n_jobs=-1, verbose=-1)
    except Exception:
        import xgboost as xgb
        return "xgboost", xgb.XGBRegressor(
            n_estimators=800, learning_rate=0.03, max_depth=6, subsample=0.8,
            colsample_bytree=0.8, reg_lambda=1.0, random_state=seed, n_jobs=-1, tree_method="hist")


def _fwd(y, target):
    return np.log(y) if target in LOG_TARGETS else y


def _inv(z, target):
    return np.exp(z) if target in LOG_TARGETS else z


def _metrics(y_true, y_pred, w):
    from sklearn.metrics import r2_score
    return dict(r2=float(r2_score(y_true, y_pred, sample_weight=w)),
                weighted_mae=float(np.average(np.abs(y_true - y_pred), weights=w)),
                weighted_rmse=float(np.sqrt(np.average((y_true - y_pred) ** 2, weights=w))))


def cross_validate(df, feats, targets, k, seed):
    from sklearn.model_selection import KFold
    splits = list(KFold(n_splits=k, shuffle=True, random_state=seed).split(df))
    report = {"k": k, "targets": {}}
    oof_all = {}
    for target in targets:
        oof = np.full(len(df), np.nan)
        for tr, te in splits:
            _, model = _make_gbm(seed)
            model.fit(df.iloc[tr][feats].values, _fwd(df.iloc[tr][target].values, target),
                      sample_weight=df.iloc[tr]["sample_weight"].values)
            oof[te] = _inv(model.predict(df.iloc[te][feats].values), target)
        report["targets"][target] = _metrics(df[target].values, oof, df["sample_weight"].values)
        oof_all[target] = oof
    return report, oof_all


def _fit_gp(X, y, seed, cap):
    from sklearn.gaussian_process import GaussianProcessRegressor
    from sklearn.gaussian_process.kernels import Matern, WhiteKernel, ConstantKernel
    from sklearn.preprocessing import StandardScaler
    from sklearn.pipeline import Pipeline
    idx = np.random.default_rng(seed).choice(X.shape[0], size=min(cap, X.shape[0]), replace=False)
    kernel = (ConstantKernel(1.0) * Matern(length_scale=np.ones(X.shape[1]), nu=2.5)
              + WhiteKernel(noise_level=1e-2))
    gp = Pipeline([("sc", StandardScaler()),
                   ("gp", GaussianProcessRegressor(kernel=kernel, normalize_y=True,
                                                   n_restarts_optimizer=2, random_state=seed))])
    gp.fit(X[idx], y[idx])
    return gp


def fit(df, feats, targets, seed, gp_cap):
    gbm_models, gp_models, backend = {}, {}, None
    for target in targets:
        backend, model = _make_gbm(seed)
        X, y, w = df[feats].values, _fwd(df[target].values, target), df["sample_weight"].values
        model.fit(X, y, sample_weight=w)
        gbm_models[target] = model
        gp_models[target] = _fit_gp(X, y, seed, gp_cap) if gp_cap > 0 else None
    return backend, gbm_models, gp_models


def train(dataset=None, out_dir="../models", k=5, seed=0, gp_cap=400, backend="mock", n=3000, jobs=1):
    dataset = ensure_dataset(dataset or "../data/pilot.parquet", backend=backend, n=n, jobs=jobs)
    df, prov = _load(dataset)
    feats = prov["feature_names"] if prov else [c for c in df.columns if c in df.columns]
    targets = [t for t in (prov["target_names"] if prov else TARGETS) if t in df.columns]
    feas = df[df["feasible"]].reset_index(drop=True)
    if len(feas) < k * 2:
        raise ValueError(f"only {len(feas)} feasible rows, need >= {k * 2}")

    cv, oof = cross_validate(feas, feats, targets, k, seed)
    gbm_backend, gbm_models, gp_models = fit(feas, feats, targets, seed, gp_cap)

    # dictionary bundle serialized for inference and plotting
    # oof holds out-of-fold predictions aligned to feasible rows
    bundle = dict(feature_names=feats, target_names=targets, gbm_backend=gbm_backend,
                  gbm_models=gbm_models, gp_models=gp_models, provenance=prov or {}, cv=cv,
                  oof={t: v.tolist() for t, v in oof.items()})
    os.makedirs(out_dir, exist_ok=True)
    import joblib
    joblib.dump(bundle, os.path.join(out_dir, "surrogate.joblib"))
    json.dump(cv, open(os.path.join(out_dir, "cv_report.json"), "w"), indent=2)

    print(f"[train] backend={gbm_backend} feasible_rows={len(feas)} k={k}")
    for t, m in cv["targets"].items():
        print(f"[train] {t:26s} R2={m['r2']:8.4f} wMAE={m['weighted_mae']:12.4g}")
    return bundle


def main(argv=None):
    p = argparse.ArgumentParser(description="train the surrogate models")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--out-dir", default="../models")
    p.add_argument("--backend", default="mock", choices=["mock", "pybind"])
    p.add_argument("--n", type=int, default=3000)
    p.add_argument("--k", type=int, default=5)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--gp-cap", type=int, default=400)
    p.add_argument("--jobs", type=int, default=1)
    a = p.parse_args(argv)
    train(a.data, out_dir=a.out_dir, k=a.k, seed=a.seed, gp_cap=a.gp_cap,
          backend=a.backend, n=a.n, jobs=a.jobs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
