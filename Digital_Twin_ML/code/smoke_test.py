"""end-to-end check using mock twin"""
import os
import tempfile

import numpy as np

from common.design_space import pilot_design_space
from common.generate_dataset import generate
from surrogate.train import train as train_surrogate
from surrogate import sensitivity, serving
from feasibility.train import train as train_feasibility

d = tempfile.mkdtemp()
ds = generate(backend="mock", n=1200, seed=0, out=os.path.join(d, "pilot.parquet"))

sur = train_surrogate(ds, out_dir=os.path.join(d, "models"), gp_cap=200)
space = pilot_design_space()
sensitivity.print_report(sensitivity.sobol_indices(sur, space, N=512), top=4)

pred = serving.Surrogate(sur, space).predict(space.decode(np.full((1, space.dim), 0.5)))
print("[serve] in_box:", pred.in_box.tolist(), "penalty:", np.round(pred.penalty, 2).tolist())

train_feasibility(ds, out_dir=os.path.join(d, "models"))
print("\nSMOKE OK")
