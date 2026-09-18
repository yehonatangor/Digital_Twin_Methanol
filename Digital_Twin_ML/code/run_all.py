"""run surrogate and feasibility pipelines end to end and write all figures

    python run_all.py                                   # mock backend
    python run_all.py --out ../runs/mock_v1             # single output directory
    python run_all.py --backend pybind --module digital_twin --out ../runs/pybind_v1
"""
from __future__ import annotations

import argparse
import os
import sys

from common.generate_dataset import generate
from common.plot_style import apply_style
from surrogate.train import train as train_surrogate
from surrogate.plots import surrogate_figs, _load_dataset as _load_sur
from feasibility.train import train as train_feasibility
from feasibility.plots import feasibility_figs, _load_dataset as _load_feas
import explore


def main(argv=None):
    p = argparse.ArgumentParser(description="run surrogate and feasibility pipelines end to end")
    p.add_argument("--backend", default="mock", choices=["mock", "pybind"])
    p.add_argument("--module", default="digital_twin")
    p.add_argument("--n", type=int, default=3000)
    p.add_argument("--out", default=None, help="output root for data, models, and figures")
    p.add_argument("--data", default="../data/pilot.parquet")
    p.add_argument("--models", default="../models")
    p.add_argument("--figures", default="../figures")
    p.add_argument("--gp-cap", type=int, default=400)
    p.add_argument("--jobs", type=int, default=1, help="parallel twin worker count")
    p.add_argument("--wide", action="store_true", help="widen box beyond operational limits")
    p.add_argument("--no-explore", action="store_true", help="skip exploration figures")
    p.add_argument("--tag", default="pilot")
    p.add_argument("--skip-generate", action="store_true", help="use existing dataset file at --data")
    a = p.parse_args(argv)

    # single output directory overrides subdirectories
    data, models, figures = a.data, a.models, a.figures
    if a.out:
        data = os.path.join(a.out, "data", "pilot.parquet")
        models = os.path.join(a.out, "models")
        figures = os.path.join(a.out, "figures")

    # load or generate dataset for training
    if a.skip_generate:
        ds = data
    else:
        ds = generate(backend=a.backend, n=a.n, out=data, module=a.module, jobs=a.jobs, wide=a.wide)
    sur = train_surrogate(ds, out_dir=models, gp_cap=a.gp_cap)
    feas = train_feasibility(ds, out_dir=models)

    apply_style()
    surrogate_figs(_load_sur(ds), sur, figures, a.tag)
    feasibility_figs(_load_feas(ds), feas, figures, a.tag)
    if not a.no_explore:
        df = _load_sur(ds)
        space = explore._space_from_provenance(sur.get("provenance"))
        explore.response_curves(df, sur, space, figures, a.tag)
        explore.sobol_bars(sur, space, figures, a.tag)
        explore.pareto(df, figures, a.tag)
        explore.surface3d(df, sur, space, figures, a.tag)
        explore.feasible_region(df, figures, a.tag)
    print("\nPIPELINE OK ->", os.path.abspath(a.out or figures))
    return 0


if __name__ == "__main__":
    sys.exit(main())
