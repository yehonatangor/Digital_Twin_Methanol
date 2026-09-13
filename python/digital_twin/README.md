# Methanol digital twin Python package

This package wraps the existing C++ `flowsheet::evaluate_design_point_full` function.
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
from digital_twin import evaluate_design_point_full, model_fingerprint

result = evaluate_design_point_full(
    prices_USD_per_MWh=[30.0, 45.0, 20.0, 55.0],
    co2_feed_kg_s=88_000 / 3_600,
    activity=1.0,
    recycle_fraction=0.70,
    reactor_inlet_T_K=483.15,
    reactor_inlet_P_bar=78.0,
    n_tubes=0,
    storage_volume_m3=5000.0,
    fresh_h2_to_co2_ratio=2.95,
    tube_inner_diameter_m=0.0,  # 0 keeps the composite-bed preset
    bed_length_m=0.0,            # 0 keeps the composite-bed preset
    n_trays=57,
)
print(result["ok"], result["message"])
if result["ok"]:
    print(result["meoh_product_kg_s"])
    print(result["unit_cost_base_USD_per_t"], result["unit_cost_full_USD_per_t"])
else:
    print(result["outcome"], result["raw_failure_message"])
print(model_fingerprint())
```

Every result includes a feasibility label and model fingerprint. Performance
targets are omitted from failed results so a zero is not mistaken for a
physical prediction. The price series drives a price-threshold dispatch
heuristic; it does not make the reactor a time-domain simulator.

The eleven sweep variables are CO2 feed rate, catalyst activity, recycle
fraction, reactor inlet temperature and pressure, tube count, H2 storage
volume, fresh H2:CO2 ratio, tube inner diameter, bed length, and column stage
count (`n_trays`). Tube count, diameter, and length use the documented
Van-Dal/Shi composite-bed value when left at zero. `n_trays` sets both the
column-sizing stage count and the shell/tray costing count. Successful results
echo all eleven resolved values with `_used` keys.

`prices_USD_per_MWh` is a required dispatch scenario, not one of those eleven
plant-design variables. `electrolyzer_power_MW`,
`price_threshold_USD_per_MWh`, and `initial_h2_kg` are optional dispatch
scenario controls; keep them fixed across a design sweep unless you are
deliberately studying dispatch scenarios too.
