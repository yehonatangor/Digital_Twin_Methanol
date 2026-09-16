import torch
from torch import nn


class ReactorProfileTransformer(nn.Module):
    def __init__(self, design_features: int = 11, output_features: int = 11,
                 hidden_size: int = 128, heads: int = 4, layers: int = 3) -> None:
        super().__init__()
        if hidden_size % heads != 0:
            raise ValueError("hidden_size must be divisible by heads")
        self.design_projection = nn.Linear(design_features, hidden_size)
        self.position_projection = nn.Linear(1, hidden_size)
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=hidden_size, nhead=heads, dim_feedforward=hidden_size * 4,
            dropout=0.1, batch_first=True, activation="gelu")
        self.encoder = nn.TransformerEncoder(encoder_layer, num_layers=layers)
        self.output = nn.Linear(hidden_size, output_features)

    def forward(self, design: torch.Tensor, position: torch.Tensor) -> torch.Tensor:
        if design.ndim != 2:
            raise ValueError(f"design must have shape (batch, features), got {design.shape}")
        if position.ndim != 3 or position.shape[-1] != 1:
            raise ValueError(f"position must have shape (batch, nodes, 1), got {position.shape}")
        design_token = self.design_projection(design).unsqueeze(1)
        position_tokens = self.position_projection(position)
        return self.output(self.encoder(position_tokens + design_token))
