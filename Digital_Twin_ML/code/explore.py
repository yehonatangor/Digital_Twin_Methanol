"""surrogate model exploration: response curves, Sobol indices, Pareto frontier, surfaces, and feasible region

    python explore.py --data ../data/pilot.parquet \
                      --model ../models/surrogate.joblib \
                      --figures ../figures
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

from common.design_space import pilot_design_space, DesignSpace, DesignVar, VarKind, Provenance
from common.plot_style import (apply_style, save, resolve_path, BLUE, AMBER, INK, GRID, MUTED,
                                CATEGORICAL, SEQUENTIAL, TARGET_LABELS as LABELS, INPUT_LABELS)
from common.twin_interface import OUTCOME_TOKENS, FEASIBLE_CODE
from surrogate.train import LOG_TARGETS
from surrogate import sensitivity


def _space_from_provenance(prov):
    # rebuild sampling box from provenance metadata
    if not prov or "design_space" not in prov:
        return pilot_design_space()
    vs = [DesignVar(r["name"], r["lo"], r["hi"], VarKind(r["kind"]),
                    Provenance(r["provenance"]), r.get("unit", ""), r.get("source", ""),
                    r.get("config_path", ""), r.get("log_scale", False))
          for r in prov["design_space"]]
    return DesignSpace(vs)


def _load(data, model):
    df = pd.read_parquet(data)
    bundle = joblib.load(model)
    space = _space_from_provenance(bundle.get("provenance"))
    return df, bundle, space


def _predict(model, X, target):
    z = model.predict(X)
    return np.exp(z) if target in LOG_TARGETS else z


def _nominal(df, feats):
    return df[df["feasible"]][feats].median().values


def _top_inputs(model, feats, k):
    imp = pd.Series(model.feature_importances_, index=feats)
    return list(imp.sort_values(ascending=False).index[:k])


def _ilabel(name):
    return INPUT_LABELS.get(name, name)


# 1. response curves
def response_curves(df, bundle, space, figdir, tag, k=6, target=None, wipe=False):
    feats = bundle["feature_names"]
    var = {v.name: v for v in space.vars}
    x0 = _nominal(df, feats)
    models = bundle["gbm_models"]
    if target:
        if target not in models:
            raise ValueError(f"unknown target {target}, must be one of {list(models.keys())}")
        models = {target: models[target]}

    for tgt, model in models.items():
        tops = _top_inputs(model, feats, k)
        fig, axes = plt.subplots(2, 3, figsize=(12, 7), sharey=True)
        for ax, name in zip(axes.ravel(), tops):
            j = feats.index(name)
            grid = np.linspace(var[name].lo, var[name].hi, 80)
            X = np.tile(x0, (len(grid), 1))
            X[:, j] = grid
            ax.plot(grid, _predict(model, X, tgt), color=BLUE, lw=2.2)
            ax.set_xlabel(_ilabel(name), fontsize=10)
        for ax in axes.ravel()[len(tops):]:
            ax.axis("off")
        for ax in axes[:, 0]:
            ax.set_ylabel(LABELS.get(tgt, tgt), fontsize=10)
        fig.suptitle(f"Response of {LABELS.get(tgt, tgt).split(' (')[0]} "
                     f"(other inputs held at median)", x=0.02, ha="left", fontsize=13, fontweight="bold")
        out_path = resolve_path(figdir, f"response_{tgt}", tag=tag, wipe=wipe)
        save(fig, out_path)


# 2. Sobol indices
def sobol_bars(bundle, space, figdir, tag, N=2048, wipe=False):
    res = sensitivity.sobol_indices(bundle, space, N=N)
    fig, axes = plt.subplots(2, 3, figsize=(13.5, 8.5))
    for ax, (target, r) in zip(axes.ravel(), res.items()):
        order = np.argsort(r.ST)
        names = [_ilabel(r.names[i]) for i in order]
        y = np.arange(len(names))
        ax.barh(y - 0.2, r.S1[order], 0.4, color=BLUE, label="first order $S_1$")
        ax.barh(y + 0.2, r.ST[order], 0.4, color=AMBER, label="total effect $S_T$")
        ax.set_yticks(y); ax.set_yticklabels(names, fontsize=8)
        ax.set_xlabel("Sobol index (-)", fontsize=10)
        ax.set_title(LABELS.get(target, target).split(" (")[0], fontsize=11)
        ax.grid(axis="y", visible=False)
    for ax in axes.ravel()[len(res):]:
        ax.axis("off")
    handles, labels = axes.ravel()[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", bbox_to_anchor=(0.98, 0.98),
               ncol=2, frameon=True, facecolor="white", edgecolor=GRID, fontsize=10)
    fig.suptitle("Global sensitivity (Sobol first-order vs total-effect)",
                 x=0.02, ha="left", fontsize=13, fontweight="bold")
    out_path = resolve_path(figdir, "sobol_indices", tag=tag, wipe=wipe)
    save(fig, out_path)


# 3. Pareto trade-off
def pareto(df, figdir, tag, wipe=False):
    feas = df[df["feasible"]]
    cost = feas["unit_cost_USD_per_t"].values
    prod = feas["annual_production_t"].values
    conv = feas["co2_conversion_overall"].values
    order = np.argsort(cost)
    best, front = -np.inf, []
    for i in order:
        if prod[i] >= best:
            front.append(i); best = prod[i]
    fig, ax = plt.subplots(figsize=(7.8, 5.8))
    sc = ax.scatter(cost, prod, c=conv, s=12, cmap=SEQUENTIAL, alpha=0.7, edgecolor="none")
    f = np.array(front)
    ax.plot(cost[f], prod[f], color=INK, lw=2, marker="o", ms=4, label="Pareto frontier")
    ax.set_xlabel(LABELS["unit_cost_USD_per_t"])
    ax.set_ylabel(LABELS["annual_production_t"])
    ax.set_title("Cost vs production trade-off")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.16), frameon=True,
              facecolor="white", edgecolor=GRID, fontsize=10)
    cb = fig.colorbar(sc, ax=ax, fraction=0.046, pad=0.04)
    cb.set_label(LABELS["co2_conversion_overall"])
    out_path = resolve_path(figdir, "pareto", tag=tag, wipe=wipe)
    save(fig, out_path)


# 4. 3D response surfaces
def surface3d(df, bundle, space, figdir, tag, targets=("co2_conversion_overall", "unit_cost_USD_per_t"),
              target=None, wipe=False):
    feats = bundle["feature_names"]
    var = {v.name: v for v in space.vars}
    x0 = _nominal(df, feats)
    if target:
        targets = (target,)
    fig = plt.figure(figsize=(14.2 if len(targets) > 1 else 7.6, 6.0))
    for idx, tgt in enumerate(targets, 1):
        model = bundle["gbm_models"][tgt]
        a, b = _top_inputs(model, feats, 2)
        ja, jb = feats.index(a), feats.index(b)
        ga = np.linspace(var[a].lo, var[a].hi, 45)
        gb = np.linspace(var[b].lo, var[b].hi, 45)
        GA, GB = np.meshgrid(ga, gb)
        X = np.tile(x0, (GA.size, 1))
        X[:, ja] = GA.ravel(); X[:, jb] = GB.ravel()
        Z = _predict(model, X, tgt).reshape(GA.shape)
        ax = fig.add_subplot(1, len(targets), idx, projection="3d")
        surf = ax.plot_surface(GA, GB, Z, cmap=SEQUENTIAL, linewidth=0.2,
                               edgecolors="#1f2937", alpha=0.92, antialiased=True)
        ax.set_xlabel(_ilabel(a), fontsize=9, labelpad=8)
        ax.set_ylabel(_ilabel(b), fontsize=9, labelpad=8)
        ax.set_title(LABELS.get(tgt, tgt).split(" (")[0], fontsize=11, pad=10)
        ax.view_init(elev=28, azim=-55)
        cb = fig.colorbar(surf, ax=ax, fraction=0.032, pad=0.12, shrink=0.85)
        cb.set_label(LABELS.get(tgt, tgt), fontsize=9.5, labelpad=8)
        cb.ax.tick_params(labelsize=8.5)
    fig.suptitle("Surrogate response surfaces (other inputs held at median)",
                 x=0.02, ha="left", fontsize=13, fontweight="bold")
    plt.tight_layout()
    suffix = f"_{target}" if target else ""
    out_path = resolve_path(figdir, f"surface3d{suffix}", tag=tag, wipe=wipe)
    save(fig, out_path)


# 5. feasible region maps
def feasible_region(df, figdir, tag, wipe=False,
                    pairs=(("reactor_inlet_P_bar", "reactor_inlet_T_K"),
                           ("fresh_h2_to_co2_ratio", "recycle_fraction"))):
    codes = sorted(c for c in df["outcome_code"].unique() if c != FEASIBLE_CODE)
    feas = df[df["feasible"]]
    fail = df[~df["feasible"]]
    fig, axes = plt.subplots(1, len(pairs), figsize=(12.5, 5.4))
    for ax, (xa, ya) in zip(np.atleast_1d(axes), pairs):
        ax.scatter(feas[xa], feas[ya], s=7, color=GRID, alpha=0.6, edgecolor="none", label="feasible")
        for c, col in zip(codes, CATEGORICAL):
            m = fail["outcome_code"] == c
            ax.scatter(fail[xa][m], fail[ya][m], s=26, color=col, edgecolor="white", linewidth=0.3,
                       label=f"{OUTCOME_TOKENS.get(c, c)} (n={int(m.sum())})")
        ax.set_xlabel(_ilabel(xa)); ax.set_ylabel(_ilabel(ya))
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", bbox_to_anchor=(0.5, -0.06),
               ncol=min(len(labels), 4), frameon=True, facecolor="white", edgecolor=GRID, fontsize=9.5)
    fig.suptitle("Feasibility and failure boundaries in design space", x=0.02, ha="left",
                 fontsize=13, fontweight="bold")
    out_path = resolve_path(figdir, "feasible_region", tag=tag, wipe=wipe)
    save(fig, out_path)


# 6. 2D contour maps with isolines and feasibility overlay
def contour2d(df, bundle, space, figdir, tag, wipe=False):
    feats = bundle["feature_names"]
    var = {v.name: v for v in space.vars}
    x0 = _nominal(df, feats)
    models = bundle["gbm_models"]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14.2, 5.8))

    # Panel 1: Overall CO2 conversion vs (Feed H2:CO2 ratio, Recycle fraction)
    m_conv = models["co2_conversion_overall"]
    r_lo, r_hi = var["fresh_h2_to_co2_ratio"].lo, var["fresh_h2_to_co2_ratio"].hi
    rf_lo, rf_hi = var["recycle_fraction"].lo, var["recycle_fraction"].hi
    gr = np.linspace(r_lo, r_hi, 60)
    grf = np.linspace(rf_lo, rf_hi, 60)
    GR, GRF = np.meshgrid(gr, grf)
    X1 = np.tile(x0, (GR.size, 1))
    X1[:, feats.index("fresh_h2_to_co2_ratio")] = GR.ravel()
    X1[:, feats.index("recycle_fraction")] = GRF.ravel()
    Z_conv = m_conv.predict(X1).reshape(GR.shape)

    cf1 = ax1.contourf(GR, GRF, 100 * Z_conv, levels=14, cmap=SEQUENTIAL, alpha=0.9)
    cs1 = ax1.contour(GR, GRF, 100 * Z_conv, levels=7, colors=INK, linewidths=0.9)
    ax1.clabel(cs1, inline=True, fontsize=8.5, fmt="%.1f%%")

    fail = df[~df["feasible"]]
    codes = sorted(c for c in df["outcome_code"].unique() if c != FEASIBLE_CODE)
    for c, col in zip(codes, CATEGORICAL):
        m = fail["outcome_code"] == c
        ax1.scatter(fail["fresh_h2_to_co2_ratio"][m], fail["recycle_fraction"][m],
                    s=22, color=col, edgecolor="white", linewidth=0.3, zorder=3,
                    label=f"{OUTCOME_TOKENS.get(c, c)} (n={int(m.sum())})")

    ax1.set_xlabel(_ilabel("fresh_h2_to_co2_ratio"))
    ax1.set_ylabel(_ilabel("recycle_fraction"))
    ax1.set_title(r"Overall $\mathrm{CO_2}$ conversion (%) with failure boundaries", fontsize=11)
    cb1 = fig.colorbar(cf1, ax=ax1, fraction=0.046, pad=0.04)
    cb1.set_label(r"Overall $\mathrm{CO_2}$ conversion (%)")
    ax1.legend(loc="lower left", frameon=True, facecolor="white", edgecolor=GRID, fontsize=8.5)

    # Panel 2: Unit production cost (USD/t) vs (CO2 feed rate, Recycle fraction)
    m_cost = models["unit_cost_USD_per_t"]
    f_lo, f_hi = var["co2_feed_kg_s"].lo, var["co2_feed_kg_s"].hi
    gf = np.linspace(f_lo, f_hi, 60)
    GF, GRF2 = np.meshgrid(gf, grf)
    X2 = np.tile(x0, (GF.size, 1))
    X2[:, feats.index("co2_feed_kg_s")] = GF.ravel()
    X2[:, feats.index("recycle_fraction")] = GRF2.ravel()
    Z_cost = np.exp(m_cost.predict(X2)).reshape(GF.shape)

    cf2 = ax2.contourf(GF, GRF2, Z_cost, levels=14, cmap="viridis_r", alpha=0.9)
    cs2 = ax2.contour(GF, GRF2, Z_cost, levels=8, colors=INK, linewidths=0.9)
    ax2.clabel(cs2, inline=True, fontsize=8.5, fmt="$%.0f")

    min_cost_rf = [grf[np.argmin(Z_cost[:, c_idx])] for c_idx in range(len(gf))]
    ax2.plot(gf, min_cost_rf, color="red", lw=1.8, ls="--", label="Optimal recycle trajectory")

    ax2.set_xlabel(_ilabel("co2_feed_kg_s"))
    ax2.set_ylabel(_ilabel("recycle_fraction"))
    ax2.set_title("Unit production cost (USD/t) and optimal recycle trajectory", fontsize=11)
    cb2 = fig.colorbar(cf2, ax=ax2, fraction=0.046, pad=0.04)
    cb2.set_label("Unit production cost (USD/t)")
    ax2.legend(loc="upper left", frameon=True, facecolor="white", edgecolor=GRID, fontsize=8.5)

    fig.suptitle("2D Quantitative response contours with operational boundaries (other inputs at median)",
                 x=0.02, ha="left", fontsize=13, fontweight="bold")
    fig.tight_layout()
    out_path = resolve_path(figdir, "contour2d", tag=tag, wipe=wipe)
    save(fig, out_path)


def main(argv=None):
    p = argparse.ArgumentParser(description="extra analysis figures")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--model", default="../models/surrogate.joblib")
    p.add_argument("--figures", default="../figures")
    p.add_argument("--tag", default="pilot")
    p.add_argument("--plot", default="all",
                   choices=["all", "response", "sobol", "pareto", "surface3d", "feasible_region", "envelope", "contour2d"],
                   help="specific exploration plot to generate")
    p.add_argument("--target", default=None, help="specific target name for response curves or surfaces")
    p.add_argument("--wipe", action="store_true", help="wipe old versions of the selected plot")
    a = p.parse_args(argv)
    apply_style()
    if a.plot == "envelope":
        from plot_envelope import load_envelope_data, plot_envelope
        for cand in ("../docs/figures/03-operating-envelope.csv", "../../docs/figures/03-operating-envelope.csv", "docs/figures/03-operating-envelope.csv"):
            if os.path.exists(cand):
                edf = load_envelope_data(cand)
                plot_envelope(edf, a.figures, tag=a.tag, wipe=a.wipe)
                return 0
        sys.exit("Error: could not find 03-operating-envelope.csv")

    if not os.path.exists(a.model):
        print(f"Model file not found at '{a.model}'. Training surrogate model from '{a.data}'...")
        from surrogate.train import train
        train(dataset=a.data, out_dir=os.path.dirname(a.model) or "../models")

    df, bundle, space = _load(a.data, a.model)
    if a.plot in ("all", "response"):
        response_curves(df, bundle, space, a.figures, a.tag, target=a.target, wipe=a.wipe)
    if a.plot in ("all", "sobol"):
        sobol_bars(bundle, space, a.figures, a.tag, wipe=a.wipe)
    if a.plot in ("all", "pareto"):
        pareto(df, a.figures, a.tag, wipe=a.wipe)
    if a.plot in ("all", "surface3d"):
        surface3d(df, bundle, space, a.figures, a.tag, target=a.target, wipe=a.wipe)
    if a.plot in ("all", "feasible_region"):
        feasible_region(df, a.figures, a.tag, wipe=a.wipe)
    if a.plot in ("all", "contour2d"):
        contour2d(df, bundle, space, a.figures, a.tag, wipe=a.wipe)
    if a.plot in ("all", "envelope"):
        from plot_envelope import load_envelope_data, plot_envelope
        for cand in ("../docs/figures/03-operating-envelope.csv", "../../docs/figures/03-operating-envelope.csv", "docs/figures/03-operating-envelope.csv"):
            if os.path.exists(cand):
                edf = load_envelope_data(cand)
                plot_envelope(edf, a.figures, tag=a.tag, wipe=a.wipe)
                break
    return 0


if __name__ == "__main__":
    sys.exit(main())
