#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <vector>

#include "flowsheet/co2_h2_plant.hpp"
#include "flowsheet/hybrid_plant_design_point.hpp"
#include "sampling/feasibility.hpp"
#include "sampling/fingerprint.hpp"
#include "sampling/validity.hpp"

namespace py = pybind11;

namespace {

py::dict evaluate_design_point(
    const std::vector<double>& prices_USD_per_MWh,
    double co2_feed_kg_s,
    double activity,
    double recycle_fraction,
    double reactor_inlet_T_K,
    double reactor_inlet_P_bar,
    double electrolyzer_power_MW,
    double price_threshold_USD_per_MWh,
    double storage_volume_m3,
    double initial_h2_kg,
    int n_tubes) {
  flowsheet::HybridPlantDesignPointConfig cfg;
  cfg.co2_feed_kg_s = co2_feed_kg_s;
  cfg.activity = activity;
  cfg.recycle_fraction = recycle_fraction;
  cfg.reactor_inlet_T_K = reactor_inlet_T_K;
  cfg.reactor_inlet_P_bar = reactor_inlet_P_bar;
  cfg.dispatch_cfg.electrolyzer_power_MW = electrolyzer_power_MW;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = price_threshold_USD_per_MWh;
  cfg.dispatch_cfg.storage.V_m3 = storage_volume_m3;
  // Leave the bed at its C++ default when n_tubes <= 0. The flowsheet then
  // resolves the documented Van-Dal/Shi composite geometry.
  if (n_tubes > 0) {
    cfg.bed = flowsheet::presets::van_dal_catalyst_shi_tubes();
    cfg.bed.n_tubes = n_tubes;
  }

  dispatch::StorageState initial_state;
  initial_state.M_H2_kg = initial_h2_kg;

  flowsheet::HybridPlantDesignPointResult result;
  {
    py::gil_scoped_release release;
    result = flowsheet::evaluate_design_point(prices_USD_per_MWh, initial_state, cfg);
  }

  auto outcome = feasibility::classify(result);
  // The current C++ classifier inspects uninitialized downstream status
  // objects before the top-level message. An early input rejection therefore
  // otherwise looks like an economics failure in Python.
  if (!result.ok &&
      (result.message == "co2_feed_kg_s must be positive" ||
       result.message == "activity must be in (0, 1]")) {
    outcome.outcome = feasibility::Outcome::InvalidInput;
    outcome.feasible = false;
    outcome.raw_message = result.message;
  }
  py::dict row;
  row["ok"] = result.ok;
  row["message"] = result.message;
  row["feasible"] = outcome.feasible;
  row["outcome_code"] = feasibility::to_code(outcome.outcome);
  row["outcome"] = feasibility::to_string(outcome.outcome);
  row["raw_failure_message"] = outcome.raw_message;
  row["model_hash"] = fingerprint::hex_hash();

  // Keep performance targets absent on failed solves: C++ initializes these
  // fields to zero, but zero would be a misleading ML training label.
  if (result.ok) {
    const auto validity_report = validity::check_design_point(cfg, result);
    row["within_validated_range"] = validity_report.within_validated_range();
    row["worst_relative_excursion"] = validity_report.worst_relative_excursion();
    row["validity_summary"] = validity_report.summary();
    row["activity_used"] = result.activity_used;
    row["n_tubes_used"] = result.bed_used.n_tubes;
    row["co2_conversion_overall"] = result.co2_conversion_overall;
    row["meoh_product_kg_s"] = result.meoh_product_kg_s;
    row["annual_production_t"] = result.annual_production_t;
    row["unit_cost_USD_per_t"] = result.unit_cost_USD_per_t;
    row["total_electricity_MW"] = result.total_electricity_MW;
    row["recycle_iterations"] = result.reactor_chain.recycle.n_iterations;
    row["recycle_residual"] = result.reactor_chain.recycle.final_residual;
  }
  return row;
}

}  // namespace

PYBIND11_MODULE(_core, module) {
  module.doc() = "Bindings for the C++ methanol plant design-point simulator";
  module.def("evaluate_design_point", &evaluate_design_point,
             py::arg("prices_USD_per_MWh"),
             py::arg("co2_feed_kg_s"),
             py::arg("activity") = 1.0,
             py::arg("recycle_fraction") = 0.70,
             py::arg("reactor_inlet_T_K") = 483.15,
             py::arg("reactor_inlet_P_bar") = 78.0,
             py::arg("electrolyzer_power_MW") = 2.0,
             py::arg("price_threshold_USD_per_MWh") = 40.0,
             py::arg("storage_volume_m3") = 5000.0,
             py::arg("initial_h2_kg") = 0.0,
             py::arg("n_tubes") = 0,
             "Evaluate one steady-state plant design point. Failed solves return diagnostics without performance targets.");
  module.def("model_fingerprint", &fingerprint::hex_hash,
             "Return the compiled model-constant fingerprint.");
}
