"""dataset row layout and file writer"""
from __future__ import annotations

import json
import math
import os
from typing import Optional

import pandas as pd

from .twin_interface import EvalResult, Fingerprint, TARGETS

WEIGHT_COL = "sample_weight"


def excursion_weight(worst_relative_excursion: float, alpha: float = 4.0) -> float:
    e = max(0.0, float(worst_relative_excursion))
    return float(math.exp(-alpha * e)) if math.isfinite(e) else 0.0


def row_from_eval(x: dict, ev: EvalResult, fp: Fingerprint, feature_names: list,
                  weight_alpha: float = 4.0) -> dict:
    row = {name: float(x[name]) for name in feature_names}
    for t in TARGETS:
        row[t] = float(ev.targets.get(t, float("nan")))
    row["feasible"] = bool(ev.feasible)
    row["outcome_code"] = int(ev.outcome_code)
    row["outcome_token"] = ev.outcome_token
    row["within_validated_range"] = bool(ev.within_validated_range)
    row["worst_relative_excursion"] = float(ev.worst_relative_excursion)
    row[WEIGHT_COL] = excursion_weight(ev.worst_relative_excursion, weight_alpha)
    row.update(fp.prov())
    return row


def build_frame(rows: list[dict]) -> pd.DataFrame:
    df = pd.DataFrame(rows)
    df["outcome_code"] = df["outcome_code"].astype("int16")
    df["feasible"] = df["feasible"].astype(bool)
    df["within_validated_range"] = df["within_validated_range"].astype(bool)
    return df


def write_dataset(df: pd.DataFrame, path: str, provenance_json: Optional[str] = None) -> str:
    # write parquet format with sidecar provenance metadata
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    base, _ = os.path.splitext(path)
    try:
        import pyarrow  # noqa: F401
        data_path = base + ".parquet"
        df.to_parquet(data_path, index=False)
    except Exception:
        data_path = base + ".csv"
        df.to_csv(data_path, index=False)
    if provenance_json:
        with open(base + ".provenance.json", "w") as f:
            json.dump({"provenance": json.loads(provenance_json)}, f, indent=2)
    return data_path
