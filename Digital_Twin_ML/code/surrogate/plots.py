"""surrogate figures: parity and feature importance

    python -m surrogate.plots
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import joblib
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

from common.plot_style import (apply_style, save, resolve_path,
                                BLUE, AMBER, INK, MUTED, GRID, TARGET_LABELS, INPUT_LABELS)
from surrogate.train import LOG_TARGETS


def _load_dataset(path):
    base, ext = os.path.splitext(path)
    return pd.read_csv(path) if ext == ".csv" else pd.read_parquet(path)


def _predict(model, X, target):
    z = model.predict(X)
    return np.exp(z) if target in LOG_TARGETS else z


def surrogate_figs(df, bundle, figdir, tag="pilot", target=None, wipe=False):
    feats = bundle["feature_names"]
    feas = df[df["feasible"]].reset_index(drop=True)
    X = feas[feats].values
    cv_targets = bundle["cv"].get("targets", {})
    oof = {t: np.asarray(v) for t, v in bundle.get("oof", {}).items()}

    models = bundle["gbm_models"]
    if target:
        if target not in models:
            raise ValueError(f"unknown target {target}, must be one of {list(models.keys())}")
        models = {target: models[target]}

    for tgt, model in models.items():
        y = feas[tgt].values
        # parity uses out-of-fold predictions when present
        pred = oof[tgt] if tgt in oof else _predict(model, X, tgt)
        label = TARGET_LABELS.get(tgt, tgt)
        cv_r2 = cv_targets.get(tgt, {}).get("r2")

        # parity scatter plot with 1:1 line
        fig, ax = plt.subplots(figsize=(6.2, 5.0))
        lo, hi = float(min(y.min(), pred.min())), float(max(y.max(), pred.max()))
        pad = 0.03 * (hi - lo)
        ax.plot([lo, hi], [lo, hi], color=MUTED, lw=1.4, ls="--", zorder=1, label="1:1 (ideal)")
        ax.scatter(y, pred, s=14, color=BLUE, alpha=0.45, edgecolor="none", zorder=2,
                   label="design points")
        ax.set_xlim(lo - pad, hi + pad)
        ax.set_ylim(lo - pad, hi + pad)
        ax.set_aspect("equal", adjustable="box")
        ax.set_xlabel(f"Twin {label}")
        ax.set_ylabel(f"Surrogate {label}")
        ax.set_title(f"Surrogate parity: {label.split(' (')[0]}")
        note = f"CV $R^2$ = {cv_r2:.3f}\n$n$ = {len(y):,}" if cv_r2 is not None else f"$n$ = {len(y):,}"
        ax.text(0.04, 0.96, note, transform=ax.transAxes, va="top", ha="left",
                bbox=dict(boxstyle="round,pad=0.45", fc="white", ec=GRID))
        ax.legend(loc="upper left", bbox_to_anchor=(1.02, 1.0), frameon=True,
                  facecolor="white", edgecolor=GRID, fontsize=9.5)
        out_path = resolve_path(figdir, f"parity_{tgt}", tag=tag, wipe=wipe)
        save(fig, out_path)

        # feature importance ranked by split count
        imp = pd.Series(model.feature_importances_, index=feats)
        imp = (imp / imp.sum()).sort_values()
        names = [INPUT_LABELS.get(k, k) for k in imp.index]
        fig, ax = plt.subplots(figsize=(6.6, 4.8))
        ax.barh(names, 100 * imp.values, color=BLUE)
        ax.set_xlabel("Relative importance (% of splits)")
        ax.set_title(f"Feature importance: {label.split(' (')[0]}")
        ax.grid(axis="y", visible=False)
        out_path = resolve_path(figdir, f"importance_{tgt}", tag=tag, wipe=wipe)
        save(fig, out_path)


def residual_figs(df, bundle, figdir, tag="pilot", target=None, wipe=False):
    feats = bundle["feature_names"]
    feas = df[df["feasible"]].reset_index(drop=True)
    X = feas[feats].values
    oof = {t: np.asarray(v) for t, v in bundle.get("oof", {}).items()}
    models = bundle["gbm_models"]
    if target:
        if target not in models:
            raise ValueError(f"unknown target {target}, must be one of {list(models.keys())}")
        models = {target: models[target]}

    targets_list = list(models.keys())
    if len(targets_list) > 1:
        fig, axes = plt.subplots(2, 3, figsize=(13.2, 7.8))
        for ax, tgt in zip(axes.ravel(), targets_list):
            y = feas[tgt].values
            pred = oof[tgt] if tgt in oof else _predict(models[tgt], X, tgt)
            res = y - pred
            pct_err = 100.0 * (res / np.clip(np.abs(y), 1e-6, None))
            mu = float(np.mean(res))
            sigma = float(np.std(res))
            p95 = float(np.percentile(np.abs(pct_err), 95))

            ax.hist(res, bins=40, density=True, color=BLUE, alpha=0.35, edgecolor=BLUE)
            grid = np.linspace(res.min(), res.max(), 200)
            try:
                from scipy.stats import gaussian_kde
                kde = gaussian_kde(res)
                ax.plot(grid, kde(grid), color=BLUE, lw=1.8, label="KDE density")
            except Exception:
                pass
            ax.axvline(0, color=INK, lw=1.2, ls="--", label="zero error")
            ax.axvline(mu, color=AMBER, lw=1.2, ls="-", label=rf"mean ($\mu$={mu:.2g})")
            ax.set_xlabel(r"Residual: $y - \hat{y}$")
            ax.set_ylabel("Probability density")
            label = TARGET_LABELS.get(tgt, tgt)
            ax.set_title(label.split(" (")[0], fontsize=10.5)
            stat_str = f"$\mu$ = {mu:.2e}\n$\sigma$ = {sigma:.2e}\n95% err = {p95:.1f}%".replace(r"\mu", r"$\mu$").replace(r"\sigma", r"$\sigma$")
            stat_str = f"$\mu$ = {mu:.2e}\n$\sigma$ = {sigma:.2e}\n95% err = {p95:.1f}%"
            ax.text(0.96, 0.94, stat_str, transform=ax.transAxes, va="top", ha="right",
                    fontsize=8.5, bbox=dict(boxstyle="round,pad=0.35", fc="white", ec=GRID))
        for ax in axes.ravel()[len(targets_list):]:
            ax.axis("off")
        handles, labels = axes.ravel()[0].get_legend_handles_labels()
        fig.legend(handles, labels, loc="upper right", bbox_to_anchor=(0.98, 0.98),
                   ncol=len(labels), frameon=True, facecolor="white", edgecolor=GRID, fontsize=9.5)
        fig.suptitle("Surrogate prediction residual distributions (out-of-fold)",
                     x=0.02, ha="left", fontsize=13, fontweight="bold")
        out_path = resolve_path(figdir, "residuals_summary", tag=tag, wipe=wipe)
        save(fig, out_path)

    for tgt, model in models.items():
        y = feas[tgt].values
        pred = oof[tgt] if tgt in oof else _predict(model, X, tgt)
        res = y - pred
        label = TARGET_LABELS.get(tgt, tgt)
        fig, (a1, a2) = plt.subplots(1, 2, figsize=(11.5, 5.0))

        a1.scatter(pred, res, s=12, color=BLUE, alpha=0.45, edgecolor="none")
        a1.axhline(0, color=INK, lw=1.2, ls="--")
        a1.set_xlabel(f"Predicted {label}")
        a1.set_ylabel(r"Residual ($y - \hat{y}$)")
        a1.set_title("Residuals vs predicted")

        pct_err = 100.0 * (res / np.clip(np.abs(y), 1e-6, None))
        a2.hist(pct_err, bins=45, color=AMBER, alpha=0.6, edgecolor=AMBER)
        a2.axvline(0, color=INK, lw=1.2, ls="--")
        a2.set_xlabel("Relative error (%)")
        a2.set_ylabel("Count")
        a2.set_title("Relative error distribution")
        p95 = float(np.percentile(np.abs(pct_err), 95))
        a2.text(0.04, 0.94, rf"95% of errors within $\pm${p95:.2f}%",
                transform=a2.transAxes, va="top", ha="left",
                fontsize=9.5, bbox=dict(boxstyle="round,pad=0.35", fc="white", ec=GRID))

        fig.suptitle(f"Error diagnostics: {label.split(' (')[0]}", x=0.02, ha="left",
                     fontsize=13, fontweight="bold")
        out_path = resolve_path(figdir, f"residuals_{tgt}", tag=tag, wipe=wipe)
        save(fig, out_path)


def main(argv=None):
    p = argparse.ArgumentParser(description="surrogate figures")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--model", default="../models/surrogate.joblib")
    p.add_argument("--figures", default="../figures")
    p.add_argument("--tag", default="pilot")
    p.add_argument("--plot", default="all", choices=["all", "parity", "importance", "residuals"],
                   help="specific plot type to generate")
    p.add_argument("--target", default=None, help="specific target name to plot")
    p.add_argument("--wipe", action="store_true", help="wipe old versions of the target plots")
    a = p.parse_args(argv)

    if not os.path.exists(a.model):
        print(f"Model file not found at '{a.model}'. Training surrogate model from '{a.data}'...")
        from surrogate.train import train
        train(dataset=a.data, out_dir=os.path.dirname(a.model) or "../models")

    apply_style()
    df = _load_dataset(a.data)
    bundle = joblib.load(a.model)
    if a.plot in ("all", "parity", "importance"):
        surrogate_figs(df, bundle, a.figures, tag=a.tag, target=a.target, wipe=a.wipe)
    if a.plot in ("all", "residuals"):
        residual_figs(df, bundle, a.figures, tag=a.tag, target=a.target, wipe=a.wipe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
