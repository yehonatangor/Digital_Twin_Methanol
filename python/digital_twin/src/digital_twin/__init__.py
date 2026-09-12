"""Python interface to the methanol plant design-point simulator."""

from ._core import evaluate_design_point, model_fingerprint

__all__ = ["evaluate_design_point", "model_fingerprint"]

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