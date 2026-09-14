"""Sampling plans over the input box."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np

from .design_space import DesignSpace


@dataclass
class SamplePlan:
    method: str
    seed: int
    n: int
    U: np.ndarray
    X: np.ndarray
    names: list[str]

    def as_dicts(self) -> list[dict]:
        return [dict(zip(self.names, row)) for row in self.X]


def _qmc():
    try:
        from scipy.stats import qmc
        return qmc
    except Exception as e:  # pragma: no cover
        raise ImportError(f"scipy is required for LHS/Sobol sampling: {e}")


def latin_hypercube(space: DesignSpace, n: int, seed: int = 0,
                    scramble: bool = True, optimization: Optional[str] = "random-cd") -> SamplePlan:
    qmc = _qmc()
    sampler = qmc.LatinHypercube(d=space.dim, seed=seed, scramble=scramble,
                                 optimization=optimization)
    U = sampler.random(n)
    return SamplePlan("latin_hypercube", seed, n, U, space.decode(U), space.names)


def sobol(space: DesignSpace, m: int, seed: int = 0, scramble: bool = True) -> SamplePlan:
    # n = 2**m points
    qmc = _qmc()
    sampler = qmc.Sobol(d=space.dim, seed=seed, scramble=scramble)
    U = sampler.random_base2(m=m)
    return SamplePlan("sobol", seed, U.shape[0], U, space.decode(U), space.names)


def pilot_plan(space: DesignSpace, budget: int = 5000, seed: int = 0,
               method: str = "lhs") -> SamplePlan:
    if method == "lhs":
        return latin_hypercube(space, budget, seed=seed)
    if method == "sobol":
        m = int(np.ceil(np.log2(budget)))
        return sobol(space, m, seed=seed)
    raise ValueError(f"unknown method {method!r}")


def screen_recycle(plan: SamplePlan, twin, R_name: str = "fresh_h2_to_co2_ratio",
                   purge_name: str = "purge_fraction") -> Optional[list[str]]:
    # flags points against the (R, purge) grid; never drops any
    if R_name not in plan.names or purge_name not in plan.names:
        return None
    iR, ip = plan.names.index(R_name), plan.names.index(purge_name)
    return [twin.recycle_feasibility(float(row[iR]), float(row[ip])) for row in plan.X]
