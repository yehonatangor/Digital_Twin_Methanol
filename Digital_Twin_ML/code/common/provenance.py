"""fingerprint stamp tying a dataset and model to one twin build"""
from __future__ import annotations

import hashlib
import json
from dataclasses import asdict, dataclass
from datetime import datetime, timezone

from .twin_interface import Fingerprint

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
    n_written: int
    created_utc: str
    contract_hash: str = ""

    def finalize(self) -> "DatasetProvenance":
        self.contract_hash = _sha(dict(fp=self.fingerprint, ds=self.design_space,
                                        feats=self.feature_names, tgts=self.target_names))
        return self

    def to_json(self) -> str:
        return json.dumps(asdict(self), indent=2, default=str)


def make_dataset_provenance(fp: Fingerprint, design_records: list, feature_names: list,
                            target_names: list, sample_method: str, seed: int,
                            n_written: int) -> DatasetProvenance:
    return DatasetProvenance(
        fingerprint=fp.prov(), design_space=design_records,
        feature_names=list(feature_names), target_names=list(target_names),
        sample_method=sample_method, seed=seed, n_written=n_written,
        created_utc=datetime.now(timezone.utc).isoformat()).finalize()


class ProvenanceMismatch(RuntimeError):
    pass


def assert_compatible(model_prov: dict, other_key: tuple) -> None:
    # refuse to serve a model whose twin fingerprint differs from the query
    model_key = tuple(model_prov.get("fingerprint", {}).get(k) for k in _FP_FIELDS)
    if model_key != other_key:
        raise ProvenanceMismatch(f"query fingerprint {other_key} != model {model_key}")
