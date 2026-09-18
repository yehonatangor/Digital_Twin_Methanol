"""plot operating envelope from converged flowsheet sweep CSV

Generates clean publication-grade figures of carbon yield vs circulator compression duty.
Replaces legacy raw SVG generator with matplotlib.
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.lines import Line2D

# adjust path for common imports if run directly
CURRENT_DIR = Path(__file__).resolve().parent
if str(CURRENT_DIR) not in sys.path:
    sys.path.insert(0, str(CURRENT_DIR))

from common.plot_style import (apply_style, save, resolve_path,
                                RATIO_COLORS, ACTIVITY_COLORS,
                                INK, MUTED, GRID, SURFACE)


def load_envelope_data(csv_path: str | Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    # filter for converged loop solutions
    converged = df[df["converged"] == 1].copy()
    converged["R"] = converged["R"].round(2)
    converged["purge_pct"] = converged["purge_pct"].round(1)
    converged["is_marginal"] = converged["portable"].astype(str).str.upper() == "MARGINAL"
    return converged


def plot_envelope(df: pd.DataFrame, out_dir: str | Path, tag: str = "pilot", wipe: bool = False):
    fig, ax = plt.subplots(figsize=(10.5, 6.8))

    # 1. shaded infeasible region where Ergun drop prevents loop closure at R=2.40
    ax.axvspan(71.5, 79.5, ymin=0.0, ymax=0.12, color="#fee2e2", alpha=0.45, zorder=0)
    ax.text(72.0, -0.35, "Infeasible loop closure (Ergun limit, purge < 3%)",
            fontsize=8.0, color="#991b1b", style="italic", va="bottom")

    # 2. constant purge lines (dashed)
    purges = [10.0, 8.0, 7.0, 5.0, 3.0, 2.0, 1.0]
    for p in purges:
        sub = df[df["purge_pct"] == p].sort_values("R")
        if len(sub) > 1:
            ax.plot(sub["carbon_yield_pct"], sub["circulator_MW"],
                    color="#9ca3af", lw=1.1, ls="--", zorder=1, alpha=0.85)
            # label purge value near right end of constant purge line
            last_pt = sub.iloc[-1]
            offset_y = 0.35 if p in (1.0, 2.0) else -0.45
            ax.annotate(f"{int(p)}%", (last_pt["carbon_yield_pct"] + 0.35, last_pt["circulator_MW"] + offset_y),
                        fontsize=8.5, color=MUTED, va="center", ha="left")

    # 3. constant feed ratio R curves (solid)
    r_values = sorted(df["R"].unique())
    for R in r_values:
        sub = df[df["R"] == R].sort_values("carbon_yield_pct")
        col = RATIO_COLORS.get(R, "#374151")
        ax.plot(sub["carbon_yield_pct"], sub["circulator_MW"],
                color=col, lw=2.4, zorder=2, label=f"Feed ratio R = {R:.2f}")

    # 4. scatter points colored by catalyst activity floor
    for _, row in df.iterrows():
        af = round(float(row["activity_floor"]), 2)
        col = ACTIVITY_COLORS.get(af, "#4b5563")
        x, y = row["carbon_yield_pct"], row["circulator_MW"]
        ax.scatter(x, y, s=60, color=col, edgecolor="white", lw=1.2, zorder=4)

        # highlight marginal non-portable convergence points with dashed outer ring
        if row["is_marginal"]:
            ax.scatter(x, y, s=140, facecolors="none", edgecolors="#dc2626",
                       lw=1.6, ls="--", zorder=5)

    # 5. highlight landmark engineering points
    landmarks = [
        {"name": "Van-Dal Design Point", "R": 2.95, "p": 1.0, "xytext": (90.0, 8.2),
         "desc": "R = 2.95, 1% purge\nYield: 97.2%, 4.8 MW"},
        {"name": "Stoichiometric Feed", "R": 3.00, "p": 1.0, "xytext": (85.5, 18.2),
         "desc": "R = 3.00, 1% purge\nYield: 98.0%, 18.9 MW (+14 MW)"},
        {"name": "Compact Loop", "R": 2.40, "p": 5.0, "xytext": (73.5, 3.8),
         "desc": "R = 2.40, 5% purge\nYield: 77.5%, 1.3 MW"},
    ]

    for lm in landmarks:
        pt = df[(df["R"] == lm["R"]) & (df["purge_pct"] == lm["p"])]
        if not pt.empty:
            px = float(pt["carbon_yield_pct"].iloc[0])
            py = float(pt["circulator_MW"].iloc[0])
            ax.scatter(px, py, s=130, facecolors="none", edgecolors=INK, lw=1.8, zorder=6)
            ax.annotate(f"{lm['name']}\n{lm['desc']}", xy=(px, py), xytext=lm["xytext"],
                        fontsize=8.5, fontweight="bold", color=INK,
                        arrowprops=dict(arrowstyle="->", color=INK, lw=1.0, shrinkA=4, shrinkB=4),
                        bbox=dict(boxstyle="round,pad=0.35", fc=SURFACE, ec=GRID, alpha=0.9),
                        zorder=7)

    # axes formatting
    ax.set_xlim(71.5, 100.5)
    ax.set_ylim(-0.5, 20.5)
    ax.set_xlabel(r"Carbon yield (% of stoichiometric ceiling, 17.80 kg/s $\mathrm{CH_3OH}$)")
    ax.set_ylabel(r"Recycle circulator compression duty (MW)")
    ax.set_title("Operating envelope: carbon yield vs loop compression duty")

    # 6. legends placed completely outside the plot area on the right
    # Legend 1: Feed ratio curves
    ratio_handles = [
        Line2D([0], [0], color=RATIO_COLORS.get(R, "#374151"), lw=2.2, label=f"R = {R:.2f}")
        for R in r_values
    ]
    ratio_handles.append(Line2D([0], [0], color="#9ca3af", lw=1.1, ls="--", label="Constant purge"))
    leg1 = ax.legend(handles=ratio_handles, title="Feed ratio $\\mathrm{H_2:CO_2}$",
                     loc="upper left", bbox_to_anchor=(1.02, 1.0), frameon=True,
                     facecolor=SURFACE, edgecolor=GRID, fontsize=9.0, title_fontsize=9.5)
    ax.add_artist(leg1)

    # Legend 2: Activity floor markers
    activity_handles = [
        Line2D([0], [0], marker="o", color="w", markerfacecolor=ACTIVITY_COLORS[a],
               markersize=8, label=f"{a:.2f}" + (" (best)" if a == 0.2 else " (vulnerable)" if a == 0.8 else ""))
        for a in sorted(ACTIVITY_COLORS.keys())
    ]
    activity_handles.append(
        Line2D([0], [0], marker="o", color="w", markerfacecolor="none",
               markeredgecolor="#dc2626", markeredgewidth=1.4, markersize=10,
               label="Marginal convergence")
    )
    ax.legend(handles=activity_handles, title="Catalyst activity floor",
              loc="upper left", bbox_to_anchor=(1.02, 0.52), frameon=True,
              facecolor=SURFACE, edgecolor=GRID, fontsize=9.0, title_fontsize=9.5)

    # save figures as PNG and SVG
    png_path = resolve_path(out_dir, "03-operating-envelope", tag=tag, ext="png", wipe=wipe)
    svg_path = resolve_path(out_dir, "03-operating-envelope", tag=tag, ext="svg", wipe=wipe)
    fig.tight_layout()
    fig.savefig(png_path, bbox_inches="tight", dpi=300)
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    print("wrote", png_path)
    print("wrote", svg_path)
    return png_path, svg_path


def main(argv=None):
    p = argparse.ArgumentParser(description="operating envelope figure generation")
    p.add_argument("--csv", default=None,
                   help="path to 03-operating-envelope.csv (auto-detected if omitted)")
    p.add_argument("--figures", default="../figures",
                   help="directory to save generated figures")
    p.add_argument("--tag", default="pilot",
                   help="figure name tag or version number")
    p.add_argument("--wipe", action="store_true",
                   help="wipe old versions of the operating envelope figure")
    a = p.parse_args(argv)

    apply_style()

    # auto-detect CSV path
    csv_candidates = [
        a.csv,
        "../docs/figures/03-operating-envelope.csv",
        "../../docs/figures/03-operating-envelope.csv",
        "docs/figures/03-operating-envelope.csv",
    ]
    csv_path = None
    for cand in csv_candidates:
        if cand and os.path.exists(cand):
            csv_path = cand
            break

    if not csv_path:
        sys.exit("Error: could not locate 03-operating-envelope.csv. Specify with --csv.")

    df = load_envelope_data(csv_path)
    plot_envelope(df, a.figures, tag=a.tag, wipe=a.wipe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
