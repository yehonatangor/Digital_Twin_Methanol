"""feasibility classifier training: calibrated binary head and multiclass failure mode head

    python -m feasibility.train
    python -m feasibility.train --backend pybind --module digital_twin
"""
from __future__ import annotations

import argparse
import json
import os
import sys

import numpy as np
import pandas as pd

from common.generate_dataset import ensure_dataset
from common.twin_interface import OUTCOME_TOKENS, TARGETS

# metadata columns excluded from model inputs
_NON_FEATURE = {"feasible", "outcome_code", "outcome_token", "within_validated_range",
                "worst_relative_excursion", "sample_weight", *TARGETS}


def _load(path):
    base, ext = os.path.splitext(path)
    df = pd.read_csv(path) if ext == ".csv" else pd.read_parquet(path)
    sidecar = base + ".provenance.json"
    prov = json.load(open(sidecar)).get("provenance") if os.path.exists(sidecar) else None
    return df, prov


def _classifier(seed, **kw):
    import lightgbm as lgb
    return lgb.LGBMClassifier(n_estimators=600, learning_rate=0.03, num_leaves=63,
                              random_state=seed, n_jobs=-1, verbose=-1, **kw)


def _oversample(X, y, seed):
    # oversample minority failure classes to balance training distribution
    from sklearn.utils import resample
    nmax = np.bincount(y).max()
    Xs, ys = [X], [y]
    for c in np.unique(y):
        idx = np.where(y == c)[0]
        if len(idx) < nmax:
            r = resample(idx, replace=True, n_samples=nmax - len(idx), random_state=seed)
            Xs.append(X[r]); ys.append(y[r])
    return np.vstack(Xs), np.concatenate(ys)


def train_binary(df, feats, seed, k=5):
    from sklearn.model_selection import StratifiedKFold, cross_val_predict
    from sklearn.calibration import CalibratedClassifierCV
    from sklearn.metrics import roc_auc_score, average_precision_score, f1_score, precision_recall_fscore_support
    X, y = df[feats].values, df["feasible"].astype(int).values
    min_count = int(np.bincount(y).min())
    cv_splits = max(2, min(k, min_count)) if min_count >= 2 else 2
    skf = StratifiedKFold(n_splits=cv_splits, shuffle=True, random_state=seed)
    oof = cross_val_predict(_classifier(seed, is_unbalance=True), X, y, cv=skf,
                            method="predict_proba", n_jobs=-1)[:, 1]
    roc, pr = float(roc_auc_score(y, oof)), float(average_precision_score(y, oof))

    # tune classification threshold for infeasible class
    infeasible = (y == 0)
    grid = np.linspace(0.05, 0.95, 181)
    f1s = [f1_score(infeasible, oof < t, zero_division=0) for t in grid]
    thr = float(grid[int(np.argmax(f1s))])

    def _infeasible_stats(t):
        p, r, f, _ = precision_recall_fscore_support(infeasible, oof < t, average="binary", zero_division=0)
        return dict(threshold=t, precision=float(p), recall=float(r), f1=float(f))

    m = dict(roc_auc=roc, pr_auc=pr, infeasible_at_0_5=_infeasible_stats(0.5),
             infeasible_at_tuned=_infeasible_stats(thr))
    print(f"[binary] out-of-fold ROC-AUC={roc:.4f} PR-AUC={pr:.4f}")
    print(f"[binary] infeasible @0.50 : P={m['infeasible_at_0_5']['precision']:.2f} "
          f"R={m['infeasible_at_0_5']['recall']:.2f} F1={m['infeasible_at_0_5']['f1']:.2f}")
    print(f"[binary] infeasible @{thr:.2f} : P={m['infeasible_at_tuned']['precision']:.2f} "
          f"R={m['infeasible_at_tuned']['recall']:.2f} F1={m['infeasible_at_tuned']['f1']:.2f}  (tuned)")

    model = CalibratedClassifierCV(_classifier(seed, is_unbalance=True), method="isotonic", cv=cv_splits)
    model.fit(X, y)
    return model, thr, m, oof


def train_multiclass(df, feats, seed, k=5):
    from sklearn.model_selection import StratifiedKFold
    from sklearn.metrics import classification_report
    X, y = df[feats].values, df["outcome_code"].astype(int).values
    _, counts = np.unique(y, return_counts=True)
    if len(counts) < 2 or counts.min() < k:
        print("[multiclass] too few failures per class, skipping")
        return None, dict(skipped=True), None

    # out-of-fold evaluation with training fold oversampled
    oof = np.empty_like(y)
    for tr, te in StratifiedKFold(n_splits=k, shuffle=True, random_state=seed).split(X, y):
        Xo, yo = _oversample(X[tr], y[tr], seed)
        clf = _classifier(seed).fit(Xo, yo)
        oof[te] = clf.predict(X[te])
    names = [OUTCOME_TOKENS.get(c, str(c)) for c in sorted(np.unique(y))]
    rep = classification_report(y, oof, target_names=names, zero_division=0)
    print("[multiclass] out-of-fold, minority-oversampled\n" + rep)

    Xo, yo = _oversample(X, y, seed)
    model = _classifier(seed).fit(Xo, yo)
    return model, dict(report=rep), oof


def train(dataset=None, out_dir="../models", seed=0, backend="mock", n=3000, jobs=1):
    dataset = ensure_dataset(dataset or "../data/pilot.parquet", backend=backend, n=n, jobs=jobs)
    df, prov = _load(dataset)
    feats = prov["feature_names"] if prov else [c for c in df.columns
                                                if c not in _NON_FEATURE and not c.startswith("fp_")]
    print(f"{len(df)} rows, feasible fraction = {df['feasible'].mean():.3f}, "
          f"failures = {int((~df['feasible']).sum())}")

    binary, thr, m_bin, oof_bin = train_binary(df, feats, seed)
    multiclass, m_mc, oof_mc = train_multiclass(df, feats, seed)

    # out-of-fold predictions aligned to dataset rows
    bundle = dict(binary=binary, binary_threshold=thr, multiclass=multiclass, features=feats,
                  provenance=prov or {}, metrics=dict(binary=m_bin, multiclass=m_mc),
                  oof_binary=oof_bin.tolist(),
                  oof_multiclass=None if oof_mc is None else oof_mc.tolist())
    os.makedirs(out_dir, exist_ok=True)
    import joblib
    joblib.dump(bundle, os.path.join(out_dir, "feasibility.joblib"))
    return bundle


def main(argv=None):
    p = argparse.ArgumentParser(description="train the feasibility classifier")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--out-dir", default="../models")
    p.add_argument("--backend", default="mock", choices=["mock", "pybind"])
    p.add_argument("--n", type=int, default=3000)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--jobs", type=int, default=1)
    a = p.parse_args(argv)
    train(a.data, out_dir=a.out_dir, seed=a.seed, backend=a.backend, n=a.n, jobs=a.jobs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
