"""feasibility figures: ROC, PR, and outcome confusion matrix

    python -m feasibility.plots
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
from sklearn.metrics import (roc_curve, auc, precision_recall_curve,
                             average_precision_score, confusion_matrix)

from common.plot_style import apply_style, save, resolve_path, BLUE, AMBER, MUTED, INK, GRID
from common.twin_interface import OUTCOME_TOKENS


def _load_dataset(path):
    base, ext = os.path.splitext(path)
    return pd.read_csv(path) if ext == ".csv" else pd.read_parquet(path)


def feasibility_figs(df, bundle, figdir, tag="pilot", seed=0, plot="all", wipe=False):
    # curves and confusion matrix from out-of-fold predictions
    y = df["feasible"].astype(int).values
    p = np.asarray(bundle["oof_binary"])

    if plot in ("all", "roc_pr"):
        fpr, tpr, _ = roc_curve(y, p)
        prec, rec, _ = precision_recall_curve(y, p)
        prevalence = float(y.mean())
        fig, (a1, a2) = plt.subplots(1, 2, figsize=(11.0, 5.2))
        a1.plot([0, 1], [0, 1], color=MUTED, lw=1.2, ls="--", label="chance")
        a1.plot(fpr, tpr, color=BLUE, lw=2.2, label=f"out-of-fold (AUC = {auc(fpr, tpr):.3f})")
        a1.set_xlim(0, 1); a1.set_ylim(0, 1.02); a1.set_aspect("equal", adjustable="box")
        a1.set_xlabel("False positive rate")
        a1.set_ylabel("True positive rate")
        a1.set_title("ROC: feasible vs infeasible")
        a1.legend(loc="upper center", bbox_to_anchor=(0.5, -0.18), frameon=True,
                  facecolor="white", edgecolor=GRID, fontsize=9.5)
        a2.axhline(prevalence, color=MUTED, lw=1.2, ls="--", label=f"baseline ({prevalence:.2f})")
        a2.plot(rec, prec, color=AMBER, lw=2.2, label=f"out-of-fold (AP = {average_precision_score(y, p):.3f})")
        a2.set_xlim(0, 1); a2.set_ylim(min(prevalence, float(prec.min())) - 0.02, 1.01)
        a2.set_xlabel("Recall")
        a2.set_ylabel("Precision")
        a2.set_title("Precision-recall")
        a2.legend(loc="upper center", bbox_to_anchor=(0.5, -0.18), frameon=True,
                  facecolor="white", edgecolor=GRID, fontsize=9.5)
        out_path = resolve_path(figdir, "feasibility_roc_pr", tag=tag, wipe=wipe)
        save(fig, out_path)

    if plot in ("all", "confusion"):
        if bundle["multiclass"] is None or bundle.get("oof_multiclass") is None:
            return
        yte = df["outcome_code"].astype(int).values
        yp = np.asarray(bundle["oof_multiclass"])
        labs = sorted(np.unique(np.concatenate([yte, yp])))
        names = [OUTCOME_TOKENS.get(c, str(c)) for c in labs]
        cm = confusion_matrix(yte, yp, labels=labs).astype(float)
        support = cm.sum(1)
        cmn = cm / np.clip(support[:, None], 1, None)
        fig, ax = plt.subplots(figsize=(7.6, 6.6))
        im = ax.imshow(cmn, cmap="Blues", vmin=0, vmax=1)
        ax.set_xticks(range(len(labs)))
        ax.set_xticklabels(names, rotation=35, ha="right", fontsize=9)
        ax.set_yticks(range(len(labs)))
        ax.set_yticklabels([f"{n} (n={int(s)})" for n, s in zip(names, support)], fontsize=9)
        ax.set_xlabel("Predicted outcome")
        ax.set_ylabel("True outcome")
        ax.set_title("Failure-mode confusion (row-normalized)")
        ax.grid(False)
        for i in range(len(labs)):
            for j in range(len(labs)):
                if cmn[i, j] > 0.005:
                    ax.text(j, i, f"{cmn[i, j]:.2f}", ha="center", va="center", fontsize=9,
                            color="white" if cmn[i, j] > 0.5 else INK)
        cb = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
        cb.set_label("Fraction of true class")
        out_path = resolve_path(figdir, "feasibility_confusion", tag=tag, wipe=wipe)
        save(fig, out_path)


def main(argv=None):
    p = argparse.ArgumentParser(description="feasibility figures")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--model", default="../models/feasibility.joblib")
    p.add_argument("--figures", default="../figures")
    p.add_argument("--tag", default="pilot")
    p.add_argument("--plot", default="all", choices=["all", "roc_pr", "confusion"],
                   help="specific feasibility plot to generate")
    p.add_argument("--wipe", action="store_true", help="wipe old versions of the selected plot")
    a = p.parse_args(argv)

    if not os.path.exists(a.model):
        print(f"Model file not found at '{a.model}'. Training feasibility model from '{a.data}'...")
        from feasibility.train import train
        train(dataset=a.data, out_dir=os.path.dirname(a.model) or "../models")

    apply_style()
    feasibility_figs(_load_dataset(a.data), joblib.load(a.model), a.figures,
                     tag=a.tag, plot=a.plot, wipe=a.wipe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
