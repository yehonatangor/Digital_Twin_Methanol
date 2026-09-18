"""sample the box, evaluate on the twin, label, and save the dataset"""
from __future__ import annotations

import argparse
import os
import sys
import time

from .design_space import pilot_design_space
from .provenance import make_dataset_provenance
from .sampling import sample
from .schema import build_frame, row_from_eval, write_dataset
from .twin_interface import get_twin, TARGETS


def generate(backend="mock", n=3000, method="lhs", seed=0, out="../data/pilot.parquet",
             weight_alpha=4.0, module="digital_twin", jobs=1, wide=False) -> str:
    space = pilot_design_space(wide=wide)
    twin = get_twin(backend, module=module) if backend == "pybind" else get_twin(backend)
    fp = twin.fingerprint()
    X = sample(space, n, seed=seed, method=method)
    samples = [dict(zip(space.names, row)) for row in X]

    t0 = time.time()
    # pybind twin releases GIL for multithreaded evaluation preserving order
    if jobs > 1:
        from concurrent.futures import ThreadPoolExecutor
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            evals = list(ex.map(twin.evaluate, samples))
    else:
        evals = [twin.evaluate(x) for x in samples]
    rows = [row_from_eval(x, ev, fp, space.names, weight_alpha) for x, ev in zip(samples, evals)]
    df = build_frame(rows)

    prov = make_dataset_provenance(fp, space.as_records(), space.names, list(TARGETS),
                                   method, seed, len(df))
    data_path = write_dataset(df, out, provenance_json=prov.to_json())

    n_feas = int(df["feasible"].sum())
    print(f"[generate] backend={backend} n={len(df)} feasible={n_feas} "
          f"({100 * n_feas / max(len(df), 1):.1f}%) {time.time() - t0:.1f}s -> {data_path}")
    print(df["outcome_token"].value_counts().to_string())
    return data_path


def ensure_dataset(path="../data/pilot.parquet", backend="mock", n=3000, module="digital_twin", jobs=1) -> str:
    # return existing dataset if present on disk
    base, _ = os.path.splitext(path)
    for cand in (base + ".parquet", base + ".csv", path):
        if os.path.exists(cand):
            return cand
    return generate(backend=backend, n=n, out=path, module=module, jobs=jobs)


def main(argv=None):
    p = argparse.ArgumentParser(description="generate the training dataset")
    p.add_argument("--backend", default="mock", choices=["mock", "pybind"])
    p.add_argument("--module", default="digital_twin")
    p.add_argument("--n", type=int, default=3000)
    p.add_argument("--method", default="lhs", choices=["lhs", "sobol"])
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--out", default="../data/pilot.parquet")
    p.add_argument("--jobs", type=int, default=1)
    p.add_argument("--wide", action="store_true", help="widen box beyond normal operating limits")
    a = p.parse_args(argv)
    generate(backend=a.backend, n=a.n, method=a.method, seed=a.seed, out=a.out,
             module=a.module, jobs=a.jobs, wide=a.wide)
    return 0


if __name__ == "__main__":
    sys.exit(main())
