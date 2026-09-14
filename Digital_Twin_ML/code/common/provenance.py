"""Fingerprint stamp tying a dataset (and a model) to one twin build."""
from __future__ import annotations

import hashlib
import json
from dataclasses import asdict, dataclass

from .twin_interface import Fingerprint

# The six-field fingerprint identity, in canonical order (dict-key form).
_FP_FIELDS = ("fp_hex", "fp_thermo", "fp_kinetics", "fp_transport", "fp_phase", "fp_numerics")


def _sha(obj) -> str:
    return hashlib.sha256(json.dumps(obj, sort_keys=True, default=str).encode()).hexdigest()[:16]


@dataclass
class DatasetProvenance:
    fingerprint: dict
    design_space: list
    feature_names: list
    target_names: list
    sample_method: str
    seed: int
    n_requested: int
    n_written: int
    tool_version: str
    created_utc: str
    contract_hash: str = ""

    def finalize(self) -> "DatasetProvenance":
        self.contract_hash = _sha(dict(
            fp=self.fingerprint.get("fp_hex"),
            fp_sub=[self.fingerprint.get(k) for k in
                    ("fp_thermo", "fp_kinetics", "fp_transport", "fp_phase", "fp_numerics")],
            ds=self.design_space, feats=self.feature_names, tgts=self.target_names,
        ))
        return self

    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2, default=str)

    def fingerprint_key(self) -> tuple:
        return tuple(self.fingerprint.get(k) for k in _FP_FIELDS)


def make_dataset_provenance(fp: Fingerprint, design_records: list, feature_names: list,
                            target_names: list, sample_method: str, seed: int,
                            n_requested: int, n_written: int, tool_version: str) -> DatasetProvenance:
    from datetime import datetime, timezone
    return DatasetProvenance(
        fingerprint=fp.as_dict(), design_space=design_records,
        feature_names=list(feature_names), target_names=list(target_names),
        sample_method=sample_method, seed=seed, n_requested=n_requested,
        n_written=n_written, tool_version=tool_version,
        created_utc=datetime.now(timezone.utc).isoformat(),
    ).finalize()


class ProvenanceMismatch(RuntimeError):
    pass


def assert_compatible(model_prov: dict, other_key: tuple, what: str = "query") -> None:
    """Raise if a model's fingerprint disagrees with the dataset or twin."""
    model_key = tuple(model_prov["fingerprint"].get(k) for k in _FP_FIELDS)
    if model_key != other_key:
        raise ProvenanceMismatch(
            f"{what} fingerprint {other_key} != model fingerprint {model_key}; "
            "the compiled constant set differs - refit or re-point the model."
        )
