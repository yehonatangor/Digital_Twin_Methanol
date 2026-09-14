"""Bridge to the compiled twin, plus a deterministic offline mock."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Optional, Protocol

import numpy as np

from .design_space import LHHW_PRESSURE_MAX_BAR, LHHW_PRESSURE_SOURCE

# Frozen feasibility::Outcome codes (feasibility.hpp). Do not renumber.
OUTCOME_TOKENS = {
    0: "Feasible", 1: "InvalidInput", 2: "ReactorTemperatureBound",
    3: "ReactorPressureBound", 4: "ReactorRhsFailure", 5: "ReactorClampViolation",
    6: "ReactorGeometryInvalid", 7: "FlashFailure", 8: "RecycleNotConverged",
    9: "CompressionFailure", 10: "EconomicsFailure", 11: "UnknownFailure",
}
FEASIBLE_CODE = 0

# Scalar targets the surrogate learns (finite on feasible rows only).
TARGETS = (
    "unit_cost_USD_per_t", "co2_conversion_overall", "meoh_product_kg_s",
    "annual_production_t", "total_electricity_MW",
)


@dataclass
class Fingerprint:
    hex: str = ""
    hash: int = 0
    n_values: int = 0
    thermo_hash: int = 0
    kinetics_hash: int = 0
    transport_hash: int = 0
    phase_hash: int = 0
    numerics_hash: int = 0

    def as_dict(self) -> dict:
        return dict(fp_hex=self.hex, fp_hash=self.hash, fp_n_values=self.n_values,
                    fp_thermo=self.thermo_hash, fp_kinetics=self.kinetics_hash,
                    fp_transport=self.transport_hash, fp_phase=self.phase_hash,
                    fp_numerics=self.numerics_hash)

    def key(self) -> tuple:
        return (self.hex, self.thermo_hash, self.kinetics_hash,
                self.transport_hash, self.phase_hash, self.numerics_hash)


@dataclass
class EvalResult:
    ok: bool
    outcome_code: int
    outcome_token: str
    raw_message: str
    targets: dict[str, float]
    within_validated_range: bool
    worst_relative_excursion: float
    excursions: list[dict] = field(default_factory=list)

    @property
    def feasible(self) -> bool:
        return self.outcome_code == FEASIBLE_CODE and self.ok


class Twin(Protocol):
    def fingerprint(self) -> Fingerprint: ...
    def evaluate(self, x: dict[str, float]) -> EvalResult: ...
    def recycle_feasibility(self, R: float, purge: float) -> str: ...


class PybindTwin:
    """Wraps the compiled twin (module defaults to 'methanol_twin')."""

    def __init__(self, module: str = "methanol_twin",
                 price_series: Optional[list[float]] = None,
                 base_config: Optional[dict] = None):
        import importlib
        self.t = importlib.import_module(module)
        self.price_series = price_series if price_series is not None else [50.0]
        self.base_config = base_config or {}

    def fingerprint(self) -> Fingerprint:
        fp = self.t.fingerprint.compute()
        return Fingerprint(
            hex=fp.hex, hash=fp.hash, n_values=fp.n_values,
            thermo_hash=fp.thermo_hash, kinetics_hash=fp.kinetics_hash,
            transport_hash=fp.transport_hash, phase_hash=fp.phase_hash,
            numerics_hash=fp.numerics_hash,
        )

    def _build_config(self, x: dict[str, float]):
        full = self.t.flowsheet.HybridPlantDesignPointFullConfig()
        cfg = full.base_cfg
        bed = cfg.bed
        disp = cfg.dispatch_cfg
        storage = disp.storage
        col = full.column_cfg

        for path, val in self.base_config.items():
            if path.startswith("__"):
                continue
            _set_path(full, path, val)

        if "n_tubes" in x: bed.n_tubes = int(x["n_tubes"])
        if "tube_inner_diameter_m" in x: bed.tube_inner_diameter_m = float(x["tube_inner_diameter_m"])
        if "bed_length_m" in x: bed.bed_length_m = float(x["bed_length_m"])
        if "total_stages" in x: col.total_stages = int(x["total_stages"])
        if "h2_storage_volume" in x: storage.V_m3 = float(x["h2_storage_volume"])
        if "reactor_inlet_T_K" in x: cfg.reactor_inlet_T_K = float(x["reactor_inlet_T_K"])
        if "reactor_inlet_P_bar" in x: cfg.reactor_inlet_P_bar = float(x["reactor_inlet_P_bar"])
        if "recycle_fraction" in x: cfg.recycle_fraction = float(x["recycle_fraction"])
        if "co2_feed_kg_s" in x: cfg.co2_feed_kg_s = float(x["co2_feed_kg_s"])
        if "fresh_h2_to_co2_ratio" in x: cfg.fresh_h2_to_co2_ratio = float(x["fresh_h2_to_co2_ratio"])
        if "activity" in x: cfg.activity = float(x["activity"])

        # Assign nested structs back (safe whether they were copies or references).
        disp.storage = storage
        cfg.dispatch_cfg = disp
        cfg.bed = bed
        full.base_cfg = cfg
        full.column_cfg = col
        return full

    def evaluate(self, x: dict[str, float]) -> EvalResult:
        full = self._build_config(x)
        storage0 = self.t.dispatch.StorageState()
        res = self.t.flowsheet.evaluate_design_point_full(self.price_series, storage0, full)
        base = res.base
        cls = self.t.feasibility.classify(base)
        rep = self.t.validity.check_design_point(full.base_cfg, base)
        exc = [dict(quantity=e.quantity, model=e.model, source=e.source, value=e.value,
                    lo=e.lo, hi=e.hi, relative=e.relative, severity=int(e.severity))
               for e in rep.excursions]
        targets = {
            "unit_cost_USD_per_t": float(res.unit_cost_full_USD_per_t),
            "co2_conversion_overall": float(base.co2_conversion_overall),
            "meoh_product_kg_s": float(base.meoh_product_kg_s),
            "annual_production_t": float(base.annual_production_t),
            "total_electricity_MW": float(base.total_electricity_MW),
        }
        return EvalResult(
            ok=bool(res.ok and base.ok),
            outcome_code=self.t.feasibility.to_code(cls.outcome),
            outcome_token=self.t.feasibility.to_string(cls.outcome),
            raw_message=cls.raw_message,
            targets=targets,
            within_validated_range=bool(rep.within_validated_range()),
            worst_relative_excursion=float(rep.worst_relative_excursion()),
            excursions=exc,
        )

    def recycle_feasibility(self, R: float, purge: float) -> str:
        rep = self.t.validity.check_recycle_loop_before_solving(R, purge)
        return self.t.validity.to_string(rep.feasibility)


def _set_path(obj: Any, path: str, val: Any) -> None:
    if not path:
        return
    parts = path.replace("cfg.", "").split(".")
    for p in parts[:-1]:
        obj = getattr(obj, p)
    setattr(obj, parts[-1], val)


class MockTwin:
    """Deterministic offline stand-in. Numbers are NOT physical."""

    SENTINEL = Fingerprint(
        hex="mock000000000000", hash=0, n_values=0,
        thermo_hash=1, kinetics_hash=2, transport_hash=3, phase_hash=4, numerics_hash=5,
    )

    def fingerprint(self) -> Fingerprint:
        return self.SENTINEL

    def recycle_feasibility(self, R: float, purge: float) -> str:
        if abs(purge - 0.01) < 5e-3 and (abs(R - 2.75) < 1e-2 or abs(R - 2.80) < 1e-2):
            return "Infeasible"
        return "Feasible"

    def evaluate(self, x: dict[str, float]) -> EvalResult:
        a = x.get("activity", 1.0)
        T = x.get("reactor_inlet_T_K", 483.15)
        P = x.get("reactor_inlet_P_bar", 78.0)
        rf = x.get("recycle_fraction", 0.70)
        F = x.get("co2_feed_kg_s", 1.0)
        infeasible_pocket = ((a < 0.5) and (rf > 0.88))
        if T > 545.0 or infeasible_pocket:
            code = 2 if T > 545.0 else 8
            return EvalResult(False, code, OUTCOME_TOKENS[code],
                              "mock: %s" % OUTCOME_TOKENS[code], {t: float("nan") for t in TARGETS},
                              False, 0.0, [])
        conv = 0.62 * a * (1 - np.exp(-(P / 60.0))) * (0.8 + 0.3 * rf)
        conv = float(np.clip(conv, 0, 0.95))
        prod = 30.0 * F * conv
        cost = 420.0 / max(conv, 1e-3) + 5.0 * (P - 50.0) + 40.0 * (1 - a)
        elec = 8.0 * F + 3.0 * rf
        worst = max(0.0, (P - LHHW_PRESSURE_MAX_BAR) / LHHW_PRESSURE_MAX_BAR)
        exc = ([dict(quantity="reactor inlet pressure", model="LHHW methanol kinetics",
                     source=LHHW_PRESSURE_SOURCE, value=P, lo=0.0, hi=LHHW_PRESSURE_MAX_BAR,
                     relative=worst, severity=0)] if worst > 0 else [])
        return EvalResult(
            True, 0, "Feasible", "",
            dict(unit_cost_USD_per_t=cost, co2_conversion_overall=conv,
                 meoh_product_kg_s=prod / (365 * 24 * 3.6), annual_production_t=prod * 24 * 365 / 1000,
                 total_electricity_MW=elec),
            within_validated_range=(worst == 0.0), worst_relative_excursion=worst, excursions=exc,
        )


def get_twin(backend: str = "mock", **kw) -> Twin:
    if backend == "mock":
        return MockTwin()
    if backend == "pybind":
        return PybindTwin(**kw)
    raise ValueError(f"unknown backend {backend!r}")
