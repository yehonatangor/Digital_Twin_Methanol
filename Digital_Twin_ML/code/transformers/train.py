from pathlib import Path

import torch
from torch import nn
from torch.utils.data import DataLoader

from dataset import load_datasets
from model import ReactorProfileTransformer


DATA_PATH = Path(__file__).resolve().parents[2] / "data" / "reactor_profiles.npz"
MODEL_PATH = Path(__file__).resolve().parents[2] / "models" / "reactor_transformer.pt"
BATCH_SIZE = 16
EPOCHS = 100
LEARNING_RATE = 1.0e-3
RANDOM_SEED = 42


def main() -> None:
    torch.manual_seed(RANDOM_SEED)
    train_dataset, validation_dataset, normalization = load_datasets(
        DATA_PATH, train_fraction=0.8, seed=RANDOM_SEED)
    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    validation_loader = DataLoader(validation_dataset, batch_size=BATCH_SIZE, shuffle=False)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on {device}")

    model_config = {
        "design_features": len(train_dataset.feature_names),
        "output_features": len(train_dataset.target_names),
        "hidden_size": 128, "heads": 4, "layers": 3,
    }
    model = ReactorProfileTransformer(**model_config).to(device)
    criterion = nn.MSELoss()
    optimizer = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=1.0e-4)
    best_validation_loss = float("inf")

    for epoch in range(1, EPOCHS + 1):
        model.train()
        training_loss = 0.0
        for batch in train_loader:
            design = batch["design"].to(device)
            position = batch["position"].to(device)
            expected = batch["profile"].to(device)
            optimizer.zero_grad()
            loss = criterion(model(design, position), expected)
            loss.backward()
            optimizer.step()
            training_loss += loss.item() * design.shape[0]
        training_loss /= len(train_dataset)

        model.eval()
        validation_loss = 0.0
        with torch.no_grad():
            for batch in validation_loader:
                design = batch["design"].to(device)
                position = batch["position"].to(device)
                expected = batch["profile"].to(device)
                loss = criterion(model(design, position), expected)
                validation_loss += loss.item() * design.shape[0]
        validation_loss /= len(validation_dataset)
        print(f"Epoch {epoch:03d}: train={training_loss:.6f}, validation={validation_loss:.6f}")

        if validation_loss < best_validation_loss:
            best_validation_loss = validation_loss
            MODEL_PATH.parent.mkdir(parents=True, exist_ok=True)
            torch.save({
                "model_state_dict": model.state_dict(),
                "model_config": model_config,
                "normalization": {
                    "x_mean": normalization.x_mean, "x_std": normalization.x_std,
                    "y_mean": normalization.y_mean, "y_std": normalization.y_std,
                },
                "feature_names": train_dataset.feature_names,
                "target_names": train_dataset.target_names,
                "best_validation_loss": best_validation_loss,
                "epoch": epoch,
            }, MODEL_PATH)

    print(f"Best validation loss: {best_validation_loss:.6f}")
    print(f"Saved model to {MODEL_PATH}")


if __name__ == "__main__":
    main()
