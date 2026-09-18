"""design variables and the sampling box"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

import numpy as np


class VarKind(Enum):
    CONTINUOUS = "continuous"
    INTEGER = "integer"


class Provenance(Enum):
    SOURCED = "sourced"
    STUDIED = "studied"


@dataclass(frozen=True)
class DesignVar:
    name: str
    lo: float
    hi: float
    kind: VarKind = VarKind.CONTINUOUS
    provenance: Provenance = Provenance.STUDIED
    unit: str = ""
    source: str = ""
    config_path: str = ""
    log_scale: bool = False


@dataclass
class DesignSpace:
    vars: list[DesignVar] = field(default_factory=list)

    def __post_init__(self):
        self._lo = np.array([np.log(v.lo) if v.log_scale else v.lo for v in self.vars])
        self._hi = np.array([np.log(v.hi) if v.log_scale else v.hi for v in self.vars])
        self._raw_lo = np.array([v.lo for v in self.vars])
        self._raw_hi = np.array([v.hi for v in self.vars])
        self._log_mask = np.array([v.log_scale for v in self.vars])
        self._int_mask = np.array([v.kind is VarKind.INTEGER for v in self.vars])

    @property
    def dim(self) -> int:
        return len(self.vars)

    @property
    def names(self) -> list[str]:
        return [v.name for v in self.vars]

    def decode(self, u: np.ndarray) -> np.ndarray:
        # map unit-cube points to real design values
        x = self._lo + np.atleast_2d(u) * (self._hi - self._lo)
        if self._log_mask.any():
            x[:, self._log_mask] = np.exp(x[:, self._log_mask])
        np.clip(x, self._raw_lo, self._raw_hi, out=x)
        if self._int_mask.any():
            x[:, self._int_mask] = np.rint(x[:, self._int_mask])
        return x

    def as_records(self) -> list[dict]:
        return [
            dict(name=v.name, lo=v.lo, hi=v.hi, kind=v.kind.value,
                 provenance=v.provenance.value, unit=v.unit, source=v.source,
                 config_path=v.config_path, log_scale=v.log_scale)
            for v in self.vars
        ]


LHHW_PRESSURE_MAX_BAR = 75.0
LHHW_PRESSURE_SOURCE = "Van-Dal & Bouallou (2013) Sec. 2.3.2 (Mignard & Pritchard 2008 refit)"


def pilot_design_space(co2_feed_nominal_kg_s: float = 1.0,
                       h2_storage_nominal_m3: float = 500.0,
                       sweep_column_stages: bool = True,
                       sweep_h2_storage: bool = True,
                       wide: bool = False) -> DesignSpace:
    # wide expands boundaries beyond standard operating window to sample physical failure modes
    P_hi = 90.0 if wide else 78.0
    T_hi = 545.0 if wide else 533.15
    recycle_hi = 0.97 if wide else 0.92
    act_lo = 0.35 if wide else 0.45
    vars_: list[DesignVar] = [
        DesignVar("n_tubes", 1000, 3000, VarKind.INTEGER, Provenance.STUDIED,
                  unit="-", config_path="base_cfg.bed.n_tubes"),
        DesignVar("tube_inner_diameter_m", 0.030, 0.050, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="m", config_path="base_cfg.bed.tube_inner_diameter_m"),
        DesignVar("bed_length_m", 5.0, 9.0, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="m", config_path="base_cfg.bed.bed_length_m"),
        DesignVar("reactor_inlet_T_K", 473.15, T_hi, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="K", source="temperature window not sourced",
                  config_path="base_cfg.reactor_inlet_T_K"),
        DesignVar("reactor_inlet_P_bar", 50.0, P_hi, VarKind.CONTINUOUS, Provenance.SOURCED,
                  unit="bar", source=LHHW_PRESSURE_SOURCE,
                  config_path="base_cfg.reactor_inlet_P_bar"),
        DesignVar("recycle_fraction", 0.60, recycle_hi, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", config_path="base_cfg.recycle_fraction"),
        DesignVar("co2_feed_kg_s", 0.5 * co2_feed_nominal_kg_s, 1.5 * co2_feed_nominal_kg_s,
                  VarKind.CONTINUOUS, Provenance.STUDIED, unit="kg/s",
                  config_path="base_cfg.co2_feed_kg_s"),
        DesignVar("fresh_h2_to_co2_ratio", 2.40, 3.00, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", source="feasible set non-convex",
                  config_path="base_cfg.fresh_h2_to_co2_ratio"),
        DesignVar("activity", act_lo, 1.00, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", config_path="base_cfg.activity"),
    ]
    if sweep_column_stages:
        vars_.append(DesignVar("total_stages", 40, 70, VarKind.INTEGER, Provenance.STUDIED,
                               unit="-", config_path="column_cfg.total_stages"))
    if sweep_h2_storage:
        vars_.append(DesignVar("h2_storage_volume", 0.5 * h2_storage_nominal_m3,
                               1.5 * h2_storage_nominal_m3, VarKind.CONTINUOUS, Provenance.STUDIED,
                               unit="m3", config_path="base_cfg.dispatch_cfg.storage.V_m3"))
    return DesignSpace(vars_)
