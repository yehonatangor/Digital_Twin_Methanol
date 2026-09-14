"""Design variables and the sampling box."""
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

    def clip(self, x: np.ndarray) -> np.ndarray:
        x = np.clip(x, self.lo, self.hi)
        if self.kind is VarKind.INTEGER:
            x = np.rint(x)
        return x


@dataclass
class DesignSpace:
    vars: list[DesignVar] = field(default_factory=list)

    def __post_init__(self):
        self._refresh()

    def _refresh(self):  # recompute if vars is mutated after construction
        self._lo = np.array([(np.log(v.lo) if v.log_scale else v.lo) for v in self.vars])
        self._hi = np.array([(np.log(v.hi) if v.log_scale else v.hi) for v in self.vars])
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

    def lower(self) -> np.ndarray:
        return self._lo

    def upper(self) -> np.ndarray:
        return self._hi

    def decode(self, u: np.ndarray) -> np.ndarray:
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


def pilot_design_space(
    co2_feed_nominal_kg_s: float = 1.0,
    h2_storage_nominal_m3: float = 500.0,
    column_stages_nominal: int = 57,
    sweep_column_stages: bool = True,
    sweep_h2_storage: bool = True,
) -> DesignSpace:
    """The 11-input pilot box; column order is frozen once a dataset exists."""
    vars_: list[DesignVar] = [
        DesignVar("n_tubes", 1000, 3000, VarKind.INTEGER, Provenance.STUDIED,
                  unit="-", source="tube count > 1 (avoids Van-Dal/Shi composite bed)",
                  config_path="base_cfg.bed.n_tubes"),
        DesignVar("tube_inner_diameter_m", 0.030, 0.050, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="m", config_path="base_cfg.bed.tube_inner_diameter_m"),
        DesignVar("bed_length_m", 5.0, 9.0, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="m", config_path="base_cfg.bed.bed_length_m"),
        DesignVar("reactor_inlet_T_K", 473.15, 533.15, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="K", source="temperature window NOT sourced (open bound); set your own range",
                  config_path="base_cfg.reactor_inlet_T_K"),
        DesignVar("reactor_inlet_P_bar", 50.0, 78.0, VarKind.CONTINUOUS, Provenance.SOURCED,
                  unit="bar", source=LHHW_PRESSURE_SOURCE + "; box straddles the 75 bar wall",
                  config_path="base_cfg.reactor_inlet_P_bar"),
        DesignVar("recycle_fraction", 0.60, 0.92, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", source="<= flux wall (co2_h2_plant_recycle)",
                  config_path="base_cfg.recycle_fraction"),
        DesignVar("co2_feed_kg_s", 0.5 * co2_feed_nominal_kg_s, 1.5 * co2_feed_nominal_kg_s,
                  VarKind.CONTINUOUS, Provenance.STUDIED, unit="kg/s",
                  config_path="base_cfg.co2_feed_kg_s"),
        DesignVar("fresh_h2_to_co2_ratio", 2.40, 3.00, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", source="realistic 2.4-3.0; feasible set non-convex (holes at R=2.75,2.80 @1% purge)",
                  config_path="base_cfg.fresh_h2_to_co2_ratio"),
        DesignVar("activity", 0.45, 1.00, VarKind.CONTINUOUS, Provenance.STUDIED,
                  unit="-", source="(0,1]; activity floor 0.45 canonical (loop stops converging)",
                  config_path="base_cfg.activity"),
    ]
    if sweep_column_stages:
        vars_.append(
            DesignVar("total_stages", 40, 70, VarKind.INTEGER, Provenance.STUDIED,
                      unit="-", source="sourced 57 (Van-Dal 44+13); swept -> studied, watch Turton size window",
                      config_path="column_cfg.total_stages"))
    if sweep_h2_storage:
        vars_.append(
            DesignVar("h2_storage_volume", 0.5 * h2_storage_nominal_m3, 1.5 * h2_storage_nominal_m3,
                      VarKind.CONTINUOUS, Provenance.STUDIED, unit="m3",
                      source="no source; near-zero effect on scalar targets at steady state (let Sobol prune)",
                      config_path="base_cfg.dispatch_cfg.storage.V_m3"))
    return DesignSpace(vars_)
