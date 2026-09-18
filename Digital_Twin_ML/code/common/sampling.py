"""space-filling sampling plans over the input box"""
from __future__ import annotations

import numpy as np
from scipy.stats import qmc

from .design_space import DesignSpace


def latin_hypercube(space: DesignSpace, n: int, seed: int = 0) -> np.ndarray:
    u = qmc.LatinHypercube(d=space.dim, seed=seed, optimization="random-cd").random(n)
    return space.decode(u)


def sobol(space: DesignSpace, n: int, seed: int = 0) -> np.ndarray:
    m = int(np.ceil(np.log2(max(n, 2))))
    u = qmc.Sobol(d=space.dim, seed=seed, scramble=True).random_base2(m=m)[:n]
    return space.decode(u)


def sample(space: DesignSpace, n: int, seed: int = 0, method: str = "lhs") -> np.ndarray:
    if method == "lhs":
        return latin_hypercube(space, n, seed)
    if method == "sobol":
        return sobol(space, n, seed)
    raise ValueError(f"unknown method {method!r}")
