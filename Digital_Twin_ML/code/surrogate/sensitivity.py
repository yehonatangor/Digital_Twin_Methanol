"""sobol first-order and total-effect indices on the fitted surrogate"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.stats import qmc

from common.design_space import DesignSpace
from surrogate.train import LOG_TARGETS


def _saltelli_matrices(space: DesignSpace, N, seed):
    d = space.dim
    base = qmc.Sobol(d=2 * d, seed=seed, scramble=True).random(N)
    Au, Bu = base[:, :d], base[:, d:]
    AB = []
    for i in range(d):
        M = Au.copy()
        M[:, i] = Bu[:, i]
        AB.append(space.decode(M))
    return space.decode(Au), space.decode(Bu), AB


def _predict(model, X, target):
    z = model.predict(X)
    return np.exp(z) if target in LOG_TARGETS else z


@dataclass
class SobolResult:
    target: str
    names: list
    S1: np.ndarray
    ST: np.ndarray

    def ranked(self):
        order = np.argsort(-self.ST)
        return [(self.names[i], float(self.S1[i]), float(self.ST[i])) for i in order]


def _sobol(yA, yB, yAB):
    varY = np.var(np.concatenate([yA, yB]), ddof=1)
    d = yAB.shape[1]
    S1, ST = np.empty(d), np.empty(d)
    for i in range(d):
        S1[i] = np.mean(yB * (yAB[:, i] - yA)) / varY
        ST[i] = 0.5 * np.mean((yA - yAB[:, i]) ** 2) / varY
    return np.clip(S1, 0, 1), np.clip(ST, 0, 1)


def sobol_indices(bundle, space: DesignSpace, N=4096, seed=0):
    A, B, AB = _saltelli_matrices(space, N, seed)
    out = {}
    for target, model in bundle["gbm_models"].items():
        yA = _predict(model, A, target)
        yB = _predict(model, B, target)
        yAB = np.column_stack([_predict(model, m, target) for m in AB])
        S1, ST = _sobol(yA, yB, yAB)
        out[target] = SobolResult(target, space.names, S1, ST)
    return out


def print_report(results, top=None):
    for target, res in results.items():
        print(f"\n[sobol] {target} (S1, ST ranked by total effect)")
        for name, s1, st in (res.ranked()[:top] if top else res.ranked()):
            print(f"[sobol] {name:24s} {s1:7.4f} {st:7.4f}")
