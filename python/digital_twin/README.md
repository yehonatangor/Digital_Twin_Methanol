# Methanol digital twin Python package

This package wraps the existing C++ `flowsheet::evaluate_design_point` function.
The C++ simulator remains in `../../include` and `../../src`; `_bindings.cpp`
is only the Python/C++ boundary.

## Structure

```text
Digital_Twin_Methanol/
  CMakeLists.txt                  # optional pybind11 target, linked to methanol_twin_core
  python/digital_twin/
    pyproject.toml               # Python build metadata
    src/digital_twin/
      __init__.py                # public Python imports
      _bindings.cpp              # pybind11 adapter
```

## Install

From `python/digital_twin`, with a C++17 compiler available:

```powershell
python -m pip install -e .
```

Build dependencies (`scikit-build-core` and `pybind11`) are installed by pip
in an isolated build environment. NumPy, pandas, and ML frameworks are not
required by this package; install them separately for your own pipeline.

## Example

```python
from digital_twin import evaluate_design_point, model_fingerprint

result = evaluate_design_point(
    prices_USD_per_MWh=[30.0, 45.0, 20.0, 55.0],
    co2_feed_kg_s=88_000 / 3_600,
    activity=1.0,
)
print(result["ok"], result["message"])
if result["ok"]:
    print(result["meoh_product_kg_s"], result["unit_cost_USD_per_t"])
else:
    print(result["outcome"], result["raw_failure_message"])
print(model_fingerprint())
```

Every result includes a feasibility label and model fingerprint. Performance
targets are omitted from failed results so a zero is not mistaken for a
physical prediction. The price series drives a price-threshold dispatch
heuristic; it does not make the reactor a time-domain simulator.
