"""shared matplotlib style, palette, and target labels for publication figures"""
from __future__ import annotations

from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt

# structural colors
INK, MUTED, GRID, SURFACE = "#1f2937", "#6b7280", "#dfe3e8", "#ffffff"
# colorblind-accessible primary accents (Okabe-Ito)
BLUE, AMBER = "#0072B2", "#E69F00"
# categorical palette for discrete failure modes
CATEGORICAL = ["#D55E00", "#0072B2", "#009E73", "#CC79A7"]
SEQUENTIAL = "viridis"

TARGET_LABELS = {
    "unit_cost_USD_per_t": "Unit cost (USD/t)",
    "co2_conversion_overall": r"Overall $\mathrm{CO_2}$ conversion (-)",
    "meoh_product_kg_s": r"Methanol production rate (kg/s)",
    "annual_production_t": r"Annual production (t/yr)",
    "total_electricity_MW": r"Total electric power (MW)",
}

INPUT_LABELS = {
    "n_tubes": "Reactor tube count (-)",
    "tube_inner_diameter_m": "Tube inner diameter (m)",
    "bed_length_m": "Catalyst bed length (m)",
    "reactor_inlet_T_K": "Inlet temperature (K)",
    "reactor_inlet_P_bar": "Inlet pressure (bar)",
    "recycle_fraction": "Recycle fraction (-)",
    "co2_feed_kg_s": r"$\mathrm{CO_2}$ feed rate (kg/s)",
    "fresh_h2_to_co2_ratio": r"Feed $\mathrm{H_2}:\mathrm{CO_2}$ ratio (-)",
    "activity": "Catalyst relative activity (-)",
    "total_stages": "Distillation stages (-)",
    "h2_storage_volume": r"$\mathrm{H_2}$ buffer storage (m$^3$)",
}


# color palette for feed ratios
RATIO_COLORS = {
    2.40: "#0072B2",
    2.70: "#009E73",
    2.85: "#E69F00",
    2.95: "#D55E00",
    3.00: "#781C6D",
}

# color palette for catalyst activity floor
ACTIVITY_COLORS = {
    0.20: "#009E73",
    0.40: "#56B4E9",
    0.60: "#E69F00",
    0.80: "#D55E00",
}


def apply_style():
    mpl.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "savefig.dpi": 300, "figure.dpi": 120,
        "font.family": "DejaVu Sans", "font.size": 12,
        "axes.titlesize": 13, "axes.titleweight": "bold", "axes.titlepad": 10,
        "axes.labelsize": 12, "axes.labelcolor": INK, "text.color": INK,
        "xtick.labelsize": 10.5, "ytick.labelsize": 10.5,
        "xtick.color": MUTED, "ytick.color": MUTED,
        "axes.edgecolor": MUTED, "axes.linewidth": 0.9,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
        "axes.spines.top": False, "axes.spines.right": False,
        "axes.titlelocation": "left", "axes.titlecolor": INK,
        "legend.frameon": True, "legend.framealpha": 0.95,
        "legend.facecolor": SURFACE, "legend.edgecolor": GRID,
        "legend.fontsize": 10.0,
        "figure.constrained_layout.use": False,
    })


def resolve_path(figdir, base_name, tag="pilot", ext="png", wipe=False) -> Path:
    target_dir = Path(figdir)
    target_dir.mkdir(parents=True, exist_ok=True)
    if wipe:
        for old in target_dir.glob(f"{base_name}*.{ext}"):
            try:
                old.unlink()
            except OSError:
                pass
    suffix = f"_{tag}" if tag else ""
    return target_dir / f"{base_name}{suffix}.{ext}"


def save(fig, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    print("wrote", path)
