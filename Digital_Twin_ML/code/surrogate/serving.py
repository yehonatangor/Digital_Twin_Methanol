"""surrogate serving with provenance verification and out-of-distribution handling"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from common.design_space import DesignSpace, LHHW_PRESSURE_MAX_BAR
from common.provenance import assert_compatible
from surrogate.train import LOG_TARGETS


@dataclass
class Prediction:
    targets: dict
    std: dict
    in_box: np.ndarray
    penalty: np.ndarray
    rejected: np.ndarray


class Surrogate:
    def __init__(self, bundle, space: DesignSpace, mode="penalize", penalty_gain=8.0):
        assert mode in ("reject", "penalize")
        self.b = bundle
        self.space = space
        self.mode = mode
        self.penalty_gain = penalty_gain

    @classmethod
    def load(cls, model_path, space, **kw):
        import joblib
        return cls(joblib.load(model_path), space, **kw)

    def check_provenance(self, twin_or_key):
        key = twin_or_key.fingerprint().key() if hasattr(twin_or_key, "fingerprint") else twin_or_key
        assert_compatible(self.b["provenance"], key)

    def _ood(self, X):
        lo = np.array([v.lo for v in self.space.vars])
        hi = np.array([v.hi for v in self.space.vars])
        width = np.where((hi - lo) > 0, hi - lo, 1.0)
        box_exc = np.max(np.clip(lo - X, 0, None) / width + np.clip(X - hi, 0, None) / width, axis=1)
        p_exc = np.zeros(len(X))
        if "reactor_inlet_P_bar" in self.space.names:
            j = self.space.names.index("reactor_inlet_P_bar")
            p_exc = np.clip(X[:, j] - LHHW_PRESSURE_MAX_BAR, 0, None) / LHHW_PRESSURE_MAX_BAR
        return box_exc <= 1e-12, box_exc, p_exc

    def predict(self, X, twin_or_key=None) -> Prediction:
        X = np.atleast_2d(np.asarray(X, dtype=float))
        if twin_or_key is not None:
            self.check_provenance(twin_or_key)
        in_box, box_exc, p_exc = self._ood(X)
        rejected = (~in_box) if self.mode == "reject" else np.zeros(len(X), bool)
        penalty = 1.0 + self.penalty_gain * (box_exc + p_exc)

        pts, stds = {}, {}
        for t, model in self.b["gbm_models"].items():
            z = model.predict(X)
            pts[t] = np.exp(z) if t in LOG_TARGETS else z
            gp = self.b["gp_models"].get(t)
            try:
                stds[t] = gp.predict(X, return_std=True)[1] * penalty if gp is not None else None
            except Exception:
                stds[t] = None
            if self.mode == "reject":
                pts[t] = np.where(rejected, np.nan, pts[t])
        return Prediction(pts, stds, in_box, penalty, rejected)
