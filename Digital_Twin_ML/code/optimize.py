"""surrogate-assisted plant optimization using surrogate and feasibility models

searches design space, filters by predicted feasibility, and ranks candidates

    python optimize.py                       # interactive objective menu
    python optimize.py --objective min_cost  # scripted
    python optimize.py --objective min_cost --verify --module digital_twin
"""
from __future__ import annotations

import argparse
import sys

import joblib
import numpy as np

from common.twin_interface import TARGETS, get_twin
from surrogate.train import LOG_TARGETS
from explore import _space_from_provenance

# objective definitions: sense, target, label
OBJECTIVES = {
    "min_cost": ("min", "unit_cost_USD_per_t", "minimize unit cost ($/t)"),
    "max_production": ("max", "annual_production_t", "maximize annual production (t)"),
    "max_conversion": ("max", "co2_conversion_overall", "maximize CO2 conversion"),
    "max_methanol": ("max", "meoh_product_kg_s", "maximize methanol rate (kg/s)"),
}


def _predict(sur, X, target):
    z = sur["gbm_models"][target].predict(X)
    return np.exp(z) if target in LOG_TARGETS else z


def _choose(argv_obj):
    # interactive prompt when terminal attached
    if argv_obj:
        return argv_obj
    if not sys.stdin.isatty():
        return "min_cost"
    print("\nObjective:")
    for i, (k, (_, _, label)) in enumerate(OBJECTIVES.items(), 1):
        print(f"  {i}. {label}")
    pick = input("choose 1-%d [1]: " % len(OBJECTIVES)).strip() or "1"
    return list(OBJECTIVES)[int(pick) - 1]


def optimize(surrogate, feasibility, space, objective, n=200000, top=5,
             p_feasible=0.5, min_production=None, seed=0):
    sense, target, label = OBJECTIVES[objective]
    rng = np.random.default_rng(seed)
    X = space.decode(rng.random((n, space.dim)))

    preds = {t: _predict(surrogate, X, t) for t in TARGETS}
    pfeas = feasibility["binary"].predict_proba(X)[:, 1]

    keep = pfeas >= p_feasible
    if min_production is not None:
        keep &= preds["annual_production_t"] >= min_production
    if not keep.any():
        raise SystemExit("no candidate met the feasibility / constraint filter; lower --p-feasible")

    score = preds[target].copy()
    score[~keep] = np.inf if sense == "min" else -np.inf
    order = np.argsort(score if sense == "min" else -score)[:top]

    print(f"\nobjective: {label}   candidates kept: {int(keep.sum())}/{n}\n")
    rows = []
    for rank, i in enumerate(order, 1):
        design = dict(zip(space.names, X[i]))
        out = {t: float(preds[t][i]) for t in TARGETS}
        rows.append((design, out, float(pfeas[i])))
        print(f"#{rank}  P(feasible)={pfeas[i]:.2f}")
        print("    outputs: " + "  ".join(f"{t}={out[t]:.4g}" for t in TARGETS))
        print("    design:  " + "  ".join(f"{k}={v:.4g}" for k, v in design.items()))
    return rows


def verify(design, module):
    twin = get_twin("pybind", module=module)
    ev = twin.evaluate(design)
    print(f"\n[verify on twin] feasible={ev.feasible} outcome={ev.outcome_token}")
    if ev.feasible:
        for t in TARGETS:
            print(f"    {t:26s} twin={ev.targets[t]:.4g}")


def main(argv=None):
    p = argparse.ArgumentParser(description="surrogate-assisted plant optimization")
    p.add_argument("--surrogate", default="../models/surrogate.joblib")
    p.add_argument("--feasibility", default="../models/feasibility.joblib")
    p.add_argument("--objective", choices=list(OBJECTIVES))
    p.add_argument("--n", type=int, default=200000)
    p.add_argument("--top", type=int, default=5)
    p.add_argument("--p-feasible", type=float, default=0.5)
    p.add_argument("--min-production", type=float, default=None)
    p.add_argument("--verify", action="store_true")
    p.add_argument("--module", default="digital_twin")
    a = p.parse_args(argv)

    sur = joblib.load(a.surrogate)
    feas = joblib.load(a.feasibility)
    space = _space_from_provenance(sur.get("provenance"))
    objective = _choose(a.objective)

    rows = optimize(sur, feas, space, objective, n=a.n, top=a.top,
                    p_feasible=a.p_feasible, min_production=a.min_production)
    if a.verify:
        verify(rows[0][0], a.module)
    print("\nNote: the optimum is a surrogate candidate. Verify on the twin before trusting it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
