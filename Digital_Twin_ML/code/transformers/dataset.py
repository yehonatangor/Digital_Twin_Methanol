from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import torch
from torch.utils.data import Dataset


@dataclass(frozen=True)
class Normalization:
    x_mean: torch.Tensor
    x_std: torch.Tensor
    y_mean: torch.Tensor
    y_std: torch.Tensor


class ReactorProfileDataset(Dataset):
    def __init__(self, X: np.ndarray, Y: np.ndarray, z: np.ndarray,
                 normalization: Normalization, feature_names: list[str],
                 target_names: list[str]) -> None:
        self.X = torch.as_tensor(X, dtype=torch.float32)
        self.Y = torch.as_tensor(Y, dtype=torch.float32)
        self.z = torch.as_tensor(z, dtype=torch.float32)
        self.normalization = normalization
        self.feature_names = tuple(feature_names)
        self.target_names = tuple(target_names)

    def __len__(self) -> int:
        return self.X.shape[0]

    def __getitem__(self, index: int) -> dict[str, torch.Tensor]:
        x = (self.X[index] - self.normalization.x_mean) / self.normalization.x_std
        y = (self.Y[index] - self.normalization.y_mean) / self.normalization.y_std
        return {"design": x, "position": self.z[:, None], "profile": y}


def calculate_normalization(X_train: np.ndarray, Y_train: np.ndarray) -> Normalization:
    if len(X_train) == 0:
        raise ValueError("cannot calculate normalization from an empty training set")
    x_mean = torch.as_tensor(X_train.mean(axis=0), dtype=torch.float32)
    x_std = torch.as_tensor(X_train.std(axis=0), dtype=torch.float32)
    y_mean = torch.as_tensor(Y_train.mean(axis=(0, 1)), dtype=torch.float32)
    y_std = torch.as_tensor(Y_train.std(axis=(0, 1)), dtype=torch.float32)
    return Normalization(x_mean, torch.clamp(x_std, min=1.0e-8),
                         y_mean, torch.clamp(y_std, min=1.0e-8))


def load_datasets(path: Path, train_fraction: float = 0.8, seed: int = 42
                  ) -> tuple[ReactorProfileDataset, ReactorProfileDataset, Normalization]:
    if not 0.0 < train_fraction < 1.0:
        raise ValueError("train_fraction must be between 0 and 1")
    with np.load(path, allow_pickle=False) as data:
        X = data["X"].copy()
        Y = data["Y"].copy()
        z = data["z_normalized"].copy()
        feature_names = data["feature_names"].tolist()
        target_names = data["target_names"].tolist()

    if X.ndim != 2 or Y.ndim != 3:
        raise ValueError(f"expected X and Y dimensions 2 and 3, got {X.shape} and {Y.shape}")
    if len(X) != len(Y):
        raise ValueError("X and Y contain different numbers of samples")
    if len(X) < 3:
        raise ValueError(f"at least 3 profiles are required, but found {len(X)}")
    if z.shape != (Y.shape[1],):
        raise ValueError(f"position grid {z.shape} does not match {Y.shape[1]} profile nodes")
    if len(feature_names) != X.shape[1] or len(target_names) != Y.shape[2]:
        raise ValueError("stored names do not match the data columns")
    if not np.isfinite(X).all() or not np.isfinite(Y).all() or not np.isfinite(z).all():
        raise ValueError("dataset contains non-finite values")

    indices = np.random.default_rng(seed).permutation(len(X))
    train_size = max(1, min(int(len(X) * train_fraction), len(X) - 1))
    train_indices, validation_indices = indices[:train_size], indices[train_size:]
    normalization = calculate_normalization(X[train_indices], Y[train_indices])
    train_dataset = ReactorProfileDataset(
        X[train_indices], Y[train_indices], z, normalization, feature_names, target_names)
    validation_dataset = ReactorProfileDataset(
        X[validation_indices], Y[validation_indices], z, normalization, feature_names, target_names)
    return train_dataset, validation_dataset, normalization
