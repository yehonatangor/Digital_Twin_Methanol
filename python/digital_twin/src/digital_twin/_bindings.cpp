#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <vector>

#include "flowsheet/co2_h2_plant.hpp"
#include "flowsheet/hybrid_plant_design_point_full.hpp"
#include "sampling/feasibility.hpp"
#include "sampling/fingerprint.hpp"
#include "sampling/validity.hpp"

namespace py = pybind11;

namespace {

py::dict evaluate_design_point_full(
    const std::vector<double>& prices_USD_per_MWh,
    double co2_feed_kg_s,
    double activity,
    double recycle_fraction,
    double reactor_inlet_T_K,
    double reactor_inlet_P_bar,
    int n_tubes,
    double storage_volume_m3,
    double fresh_h2_to_co2_ratio,
    double tube_inner_diameter_m,
    double bed_length_m,
    int n_trays,
    double electrolyzer_power_MW,
    double price_threshold_USD_per_MWh,
    double initial_h2_kg) {
  if (tube_inner_diameter_m < 0.0 || bed_length_m < 0.0 || n_tubes < 0 || n_trays <= 0)
    throw py::value_error("geometry overrides must be non-negative and n_trays must be positive");

  flowsheet::HybridPlantDesignPointFullConfig fcfg;
  auto& cfg = fcfg.base_cfg;

  cfg.co2_feed_kg_s = co2_feed_kg_s;
  cfg.activity = activity;
  cfg.recycle_fraction = recycle_fraction;
  cfg.reactor_inlet_T_K = reactor_inlet_T_K;
  cfg.reactor_inlet_P_bar = reactor_inlet_P_bar;
  cfg.fresh_h2_to_co2_ratio = fresh_h2_to_co2_ratio;

  cfg.dispatch_cfg.storage.V_m3 = storage_volume_m3;
  cfg.dispatch_cfg.electrolyzer_power_MW = electrolyzer_power_MW;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = price_threshold_USD_per_MWh;

  fcfg.n_trays = n_trays;
  fcfg.column_cfg.total_stages = n_trays;

  // Zero means "use the composite preset" for each geometry input. Resolve
  // the preset first so any one dimension can be swept independently.

  if (n_tubes > 0 || tube_inner_diameter_m > 0.0 || bed_length_m > 0.0) {
    cfg.bed = flowsheet::presets::van_dal_catalyst_shi_tubes();

    if (n_tubes > 0) cfg.bed.n_tubes = n_tubes;

    if (tube_inner_diameter_m > 0.0)
      cfg.bed.tube_inner_diameter_m = tube_inner_diameter_m;

    if (bed_length_m > 0.0) cfg.bed.bed_length_m = bed_length_m;

  }

  dispatch::StorageState initial_state;
  initial_state.M_H2_kg = initial_h2_kg;

  flowsheet::HybridPlantDesignPointFullResult result;
  {
    py::gil_scoped_release release;
    result = flowsheet::evaluate_design_point_full(
      prices_USD_per_MWh, initial_state, fcfg);
  }

  auto outcome = feasibility::classify(result.base);
  // The current C++ classifier inspects uninitialized downstream status
  // objects before the top-level message. An early input rejection therefore
  // otherwise looks like an economics failure in Python.
  if (!result.base.ok &&
      (result.base.message == "co2_feed_kg_s must be positive" ||
       result.base.message == "activity must be in (0, 1]")) {
    outcome.outcome = feasibility::Outcome::InvalidInput;
    outcome.feasible = false;
  } else if (result.base.ok && !result.ok) {
    // Vessel/column sizing or full CAPEX failed after the base plant solved.
    outcome.outcome = result.message.find("CAPEX") != std::string::npos
                          ? feasibility::Outcome::EconomicsFailure
                          : feasibility::Outcome::UnknownFailure;
    outcome.feasible = false;
  }

  if (!result.ok) outcome.raw_message = result.message;

  py::dict row;
  row["ok"] = result.ok;
  row["message"] = result.message;
  row["feasible"] = result.ok;
  row["outcome_code"] = feasibility::to_code(outcome.outcome);
  row["outcome"] = feasibility::to_string(outcome.outcome);
  row["raw_failure_message"] = outcome.raw_message;
  row["model_hash"] = fingerprint::hex_hash();

  // Keep performance targets absent on failed solves: C++ initializes these
  // fields to zero, but zero would be a misleading ML training label.
  if (result.ok) {
    const auto validity_report = validity::check_design_point(cfg, result.base);
    row["within_validated_range"] = validity_report.within_validated_range();
    row["worst_relative_excursion"] = validity_report.worst_relative_excursion();
    row["validity_summary"] = validity_report.summary();
    row["co2_feed_kg_s_used"] = cfg.co2_feed_kg_s;
    row["activity_used"] = result.base.activity_used;
    row["recycle_fraction_used"] = cfg.recycle_fraction;
    row["reactor_inlet_T_K_used"] = cfg.reactor_inlet_T_K;
    row["reactor_inlet_P_bar_used"] = cfg.reactor_inlet_P_bar;
    row["n_tubes_used"] = result.base.bed_used.n_tubes;
    row["storage_volume_m3_used"] = cfg.dispatch_cfg.storage.V_m3;
    row["fresh_h2_to_co2_ratio_used"] = cfg.fresh_h2_to_co2_ratio;
    row["tube_inner_diameter_m_used"] = result.base.bed_used.tube_inner_diameter_m;
    row["bed_length_m_used"] = result.base.bed_used.bed_length_m;
    row["n_trays_used"] = fcfg.n_trays;
    row["co2_conversion_overall"] = result.base.co2_conversion_overall;
    row["meoh_product_kg_s"] = result.base.meoh_product_kg_s;
    row["annual_production_t"] = result.base.annual_production_t;
    // Retain the old key as an alias for the full-scope cost.
    row["unit_cost_USD_per_t"] = result.unit_cost_full_USD_per_t;
    row["total_electricity_MW"] = result.base.total_electricity_MW;
    row["recycle_iterations"] = result.base.reactor_chain.recycle.n_iterations;
    row["recycle_residual"] = result.base.reactor_chain.recycle.final_residual;

    row["unit_cost_base_USD_per_t"] = result.unit_cost_base_USD_per_t;
    row["unit_cost_full_USD_per_t"] = result.unit_cost_full_USD_per_t;
  }
  return row;
}

}  // namespace

PYBIND11_MODULE(_core, module) {
  module.doc() = "Bindings for the C++ methanol plant design-point simulator";
  module.def("evaluate_design_point_full", &evaluate_design_point_full,
             py::arg("prices_USD_per_MWh"),
             py::arg("co2_feed_kg_s"),
             py::arg("activity") = 1.0,
             py::arg("recycle_fraction") = 0.70,
             py::arg("reactor_inlet_T_K") = 483.15,
             py::arg("reactor_inlet_P_bar") = 78.0,
             py::arg("n_tubes") = 0,
             py::arg("storage_volume_m3") = 5000.0,
             py::arg("fresh_h2_to_co2_ratio") = 2.95,
             py::arg("tube_inner_diameter_m") = 0.0,
             py::arg("bed_length_m") = 0.0,
             py::arg("n_trays") = 57,
             py::kw_only(),
             py::arg("electrolyzer_power_MW") = 2.0,
             py::arg("price_threshold_USD_per_MWh") = 40.0,
             py::arg("initial_h2_kg") = 0.0,
             "Evaluate one steady-state plant design point. Failed solves return diagnostics without performance targets.");
  module.def("model_fingerprint", &fingerprint::hex_hash,
             "Return the compiled model-constant fingerprint.");
}
