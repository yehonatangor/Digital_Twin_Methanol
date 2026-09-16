"""Generate fixed-grid reactor profiles for operator-learning models."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

import numpy as np

from digital_twin import evaluate_design_point_full, model_fingerprint


FEATURE_NAMES = (
    "co2_feed_kg_s", "activity", "recycle_fraction", "reactor_inlet_T_K",
    "reactor_inlet_P_bar", "n_tubes", "storage_volume_m3",
    "fresh_h2_to_co2_ratio", "tube_inner_diameter_m", "bed_length_m", "n_trays",
)
PRICES_USD_PER_MWH = (30.0,) * 24
DEFAULT_OUTPUT = Path(__file__).resolve().parents[3] / "data" / "reactor_profiles.npz"
DEFAULT_SAMPLES = 20
DEFAULT_PROFILE_NODES = 128
DEFAULT_SEED = 42


def sample_designs(count: int, seed: int) -> list[dict[str, float | int]]:
    """Sample a small pilot region around the known plant design."""
    if count <= 0:
        raise ValueError("count must be positive")
    rng = np.random.default_rng(seed)
    return [
        {
            "co2_feed_kg_s": rng.uniform(22.0, 27.0),
            "activity": rng.uniform(0.8, 1.0),
            "recycle_fraction": rng.uniform(0.65, 0.75),
            "reactor_inlet_T_K": rng.uniform(478.0, 488.0),
            "reactor_inlet_P_bar": rng.uniform(60, 75.0),
            "n_tubes": int(rng.integers(5900, 6501)),
            "storage_volume_m3": rng.uniform(4500.0, 5500.0),
            "fresh_h2_to_co2_ratio": rng.uniform(2.90, 3.00),
            "tube_inner_diameter_m": rng.uniform(0.033, 0.037),
            "bed_length_m": rng.uniform(6.8, 7.2),
            "n_trays": int(rng.integers(55, 60)),
        }
        for _ in range(count)
    ]


def interpolate_profile(result: dict[str, Any], nodes: int) -> np.ndarray:
    """Interpolate one successful raw profile onto a normalized axial grid."""
    if nodes < 2:
        raise ValueError("nodes must be at least 2")
    profile = result["reactor_profile"]
    species = result["reactor_species"]
    if len(profile) < 2:
        raise ValueError("reactor profile must contain at least two points")

    z_m = np.asarray([point["z_m"] for point in profile], dtype=np.float64)
    states = np.asarray(
        [[point["T_K"], point["P_bar"], *point["mole_fractions"]] for point in profile],
        dtype=np.float64,
    )
    expected_columns = 2 + len(species)
    if states.shape != (len(profile), expected_columns):
        raise ValueError(f"unexpected raw profile shape {states.shape}")
    if not np.isfinite(z_m).all() or not np.isfinite(states).all():
        raise ValueError("reactor profile contains non-finite values")
    if z_m[0] < 0.0 or z_m[-1] <= 0.0:
        raise ValueError("reactor profile has an invalid axial range")
    if np.any(np.diff(z_m) <= 0.0):
        raise ValueError("reactor axial positions must be strictly increasing")

    normalized_z = z_m / z_m[-1]
    target_grid = np.linspace(0.0, 1.0, nodes, dtype=np.float64)
    return np.column_stack([
        np.interp(target_grid, normalized_z, states[:, column])
        for column in range(states.shape[1])
    ])


def generate_dataset(sample_count: int, profile_nodes: int, seed: int, output_path: Path) -> None:
    designs = sample_designs(sample_count, seed)
    expected_hash = model_fingerprint()
    expected_species: tuple[str, ...] | None = None
    input_rows: list[list[float]] = []
    profile_rows: list[np.ndarray] = []
    failure_indices: list[int] = []
    failure_outcomes: list[str] = []
    failure_messages: list[str] = []

    for index, design in enumerate(designs):
        result: dict[str, Any] | None = None
        try:
            result = evaluate_design_point_full(
                prices_USD_per_MWh=PRICES_USD_PER_MWH,
                **design,
                include_reactor_profile=True,
            )
            if not result.get("ok", False):
                raise RuntimeError(
                    f"{result.get('outcome', 'unknown')}: "
                    f"{result.get('message', 'simulation failed')}"
                )
            if result.get("model_hash") != expected_hash:
                raise RuntimeError("model fingerprint changed during generation")

            species = tuple(result["reactor_species"])
            if expected_species is None:
                expected_species = species
            elif species != expected_species:
                raise RuntimeError("species order changed between simulations")
            interpolated = interpolate_profile(result, profile_nodes)
        except Exception as exc:
            failure_indices.append(index)
            failure_outcomes.append(
                str(result.get("outcome", "python_exception"))
                if result is not None else "python_exception"
            )
            failure_messages.append(str(exc))
            print(f"{index + 1}/{sample_count}: failed: {exc}")
            continue

        input_rows.append([float(design[name]) for name in FEATURE_NAMES])
        profile_rows.append(interpolated)
        print(f"{index + 1}/{sample_count}: success ({len(result['reactor_profile'])} raw nodes)")

    if not input_rows or expected_species is None:
        raise RuntimeError("no valid reactor profiles were generated")

    X = np.asarray(input_rows, dtype=np.float64)
    Y = np.asarray(profile_rows, dtype=np.float64)
    expected_shape = (len(input_rows), profile_nodes, 2 + len(expected_species))
    if X.shape != (len(input_rows), len(FEATURE_NAMES)):
        raise RuntimeError(f"unexpected input shape {X.shape}")
    if Y.shape != expected_shape:
        raise RuntimeError(f"unexpected target shape {Y.shape}")
    if not np.isfinite(X).all() or not np.isfinite(Y).all():
        raise RuntimeError("generated dataset contains non-finite values")

    mole_fractions = Y[:, :, 2:]
    if np.any(mole_fractions < -1.0e-10):
        raise RuntimeError("generated profiles contain negative mole fractions")
    maximum_sum_error = float(np.max(np.abs(mole_fractions.sum(axis=-1) - 1.0)))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        output_path,
        X=X,
        Y=Y,
        z_normalized=np.linspace(0.0, 1.0, profile_nodes, dtype=np.float64),
        feature_names=np.asarray(FEATURE_NAMES),
        target_names=np.asarray(("T_K", "P_bar", *expected_species)),
        species_names=np.asarray(expected_species),
        model_hash=np.asarray(expected_hash),
        prices_USD_per_MWh=np.asarray(PRICES_USD_PER_MWH, dtype=np.float64),
        random_seed=np.asarray(seed),
        n_requested=np.asarray(sample_count),
        n_successful=np.asarray(len(input_rows)),
        failure_indices=np.asarray(failure_indices, dtype=np.int64),
        failure_outcomes=np.asarray(failure_outcomes),
        failure_messages=np.asarray(failure_messages),
    )

    print(f"Saved {len(input_rows)} profiles to {output_path}")
    print(f"Failed simulations: {len(failure_indices)}")
    print(f"X shape: {X.shape}")
    print(f"Y shape: {Y.shape}")
    print(f"Maximum composition-sum error: {maximum_sum_error:.3e}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    parser.add_argument("--nodes", type=int, default=DEFAULT_PROFILE_NODES)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    generate_dataset(args.samples, args.nodes, args.seed, args.output)


if __name__ == "__main__":
    main()
