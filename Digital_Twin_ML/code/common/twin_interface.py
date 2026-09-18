"""bridge to the compiled twin plus a deterministic offline mock"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Protocol

import numpy as np

from .design_space import LHHW_PRESSURE_MAX_BAR

# frozen feasibility::Outcome codes, do not renumber
OUTCOME_TOKENS = {
    0: "Feasible", 1: "InvalidInput", 2: "ReactorTemperatureBound",
    3: "ReactorPressureBound", 4: "ReactorRhsFailure", 5: "ReactorClampViolation",
    6: "ReactorGeometryInvalid", 7: "FlashFailure", 8: "RecycleNotConverged",
    9: "CompressionFailure", 10: "EconomicsFailure", 11: "UnknownFailure",
}
FEASIBLE_CODE = 0

# scalar targets the surrogate learns
TARGETS = (
    "unit_cost_USD_per_t", "co2_conversion_overall", "meoh_product_kg_s",
    "annual_production_t", "total_electricity_MW",
)


@dataclass
class Fingerprint:
    hex: str = ""
    thermo_hash: int = 0
    kinetics_hash: int = 0
    transport_hash: int = 0
    phase_hash: int = 0
    numerics_hash: int = 0

    def prov(self) -> dict:
        return {"fp_hex": self.hex, "fp_thermo": self.thermo_hash,
                "fp_kinetics": self.kinetics_hash, "fp_transport": self.transport_hash,
                "fp_phase": self.phase_hash, "fp_numerics": self.numerics_hash}

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

    @property
    def feasible(self) -> bool:
        return self.outcome_code == FEASIBLE_CODE and self.ok


class Twin(Protocol):
    def fingerprint(self) -> Fingerprint: ...
    def evaluate(self, x: dict[str, float]) -> EvalResult: ...


# design-var names that differ from the binding's keyword args
_ARG_MAP = {"total_stages": "n_trays", "h2_storage_volume": "storage_volume_m3"}
_INT_ARGS = {"n_tubes", "n_trays"}


class PybindTwin:
    """wraps the compiled digital_twin package (pip install -e python/digital_twin)"""

    def __init__(self, module: str = "digital_twin",
                 price_series: Optional[list[float]] = None,
                 scenario: Optional[dict] = None):
        import importlib
        self.t = importlib.import_module(module)
        # fixed dispatch scenario, held constant across a design sweep
        self.price_series = price_series if price_series is not None else [30.0, 45.0, 20.0, 55.0]
        self.scenario = scenario or {}

    def fingerprint(self) -> Fingerprint:
        return Fingerprint(hex=str(self.t.model_fingerprint()))

    def evaluate(self, x: dict[str, float]) -> EvalResult:
        kw = {_ARG_MAP.get(name, name): val for name, val in x.items()}
        for a in _INT_ARGS:
            if a in kw:
                kw[a] = int(round(kw[a]))
        row = self.t.evaluate_design_point_full(self.price_series, **kw, **self.scenario)
        targets = {t: float(row[t]) if t in row else float("nan") for t in TARGETS}
        return EvalResult(
            ok=bool(row["ok"]),
            outcome_code=int(row["outcome_code"]),
            outcome_token=str(row["outcome"]),
            raw_message=str(row.get("raw_failure_message", "")),
            targets=targets,
            within_validated_range=bool(row.get("within_validated_range", False)),
            worst_relative_excursion=float(row.get("worst_relative_excursion", 0.0)),
        )


class MockTwin:
    """deterministic offline stand-in, numbers are not physical"""

    SENTINEL = Fingerprint("mock000000000000", 1, 2, 3, 4, 5)

    def fingerprint(self) -> Fingerprint:
        return self.SENTINEL

    def evaluate(self, x: dict[str, float]) -> EvalResult:
        a = x.get("activity", 1.0)
        T = x.get("reactor_inlet_T_K", 483.15)
        P = x.get("reactor_inlet_P_bar", 78.0)
        rf = x.get("recycle_fraction", 0.70)
        F = x.get("co2_feed_kg_s", 1.0)
        # synthetic failure pocket to sample minority failure classes
        if T > 545.0 or (a < 0.5 and rf > 0.88):
            code = 2 if T > 545.0 else 8
            return EvalResult(False, code, OUTCOME_TOKENS[code], "mock: " + OUTCOME_TOKENS[code],
                              {t: float("nan") for t in TARGETS}, False, 0.0)
        conv = float(np.clip(0.62 * a * (1 - np.exp(-(P / 60.0))) * (0.8 + 0.3 * rf), 0, 0.95))
        prod = 30.0 * F * conv
        cost = 420.0 / max(conv, 1e-3) + 5.0 * (P - 50.0) + 40.0 * (1 - a)
        elec = 8.0 * F + 3.0 * rf
        worst = max(0.0, (P - LHHW_PRESSURE_MAX_BAR) / LHHW_PRESSURE_MAX_BAR)
        return EvalResult(
            True, 0, "Feasible", "",
            dict(unit_cost_USD_per_t=cost, co2_conversion_overall=conv,
                 meoh_product_kg_s=prod / (365 * 24 * 3.6),
                 annual_production_t=prod * 24 * 365 / 1000, total_electricity_MW=elec),
            within_validated_range=(worst == 0.0), worst_relative_excursion=worst)


def get_twin(backend: str = "mock", **kw) -> Twin:
    if backend == "mock":
        return MockTwin()
    if backend == "pybind":
        return PybindTwin(**kw)
    raise ValueError(f"unknown backend {backend!r}")
