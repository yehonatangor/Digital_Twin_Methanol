#pragma once
// Price-threshold electrolyser dispatch over a hydrogen storage buffer

// At each timestep: run the electrolyser at its configured power if price is at or below the threshold
// otherwise idle it and draw the downstream hydrogen demand from storage

// This is a myopic heuristic, NOT Mucci's optimiser
// Their flexible-operation results come from a full horizon optimisation whose formulation is in
// supplementary material outside this library
// Results here are not compared against, and must not be read as reproducing, any of Mucci's reported figures

// Composes front_end/electrolyzer.hpp and dispatch/storage.hpp unchanged; adds no new physics
// Price series is caller-supplied, USD/MWh; no data is fabricated here and no network call is made anywhere
// See docs/22-storage-and-dispatch.md

#include <vector>
#include <string>
#include "front_end/electrolyzer.hpp"
#include "dispatch/storage.hpp"

namespace dispatch {

struct DispatchConfig {
  double price_threshold_USD_per_MWh = 0.0;  // electrolyzer runs "on" when price <= this -- no default, caller's own choice
  front_end::ElectrolyzerConfig electrolyzer_cfg;  // reuses Chapter 6.1's module unmodified; defaults = Mucci Table 2/A.1
  double electrolyzer_power_MW = 0.0; // P_PEM_MW dispatched when "on" (0 = disabled, caller must set)
  double electrolyzer_pressure_bar = 30.0; // p_PEM_bar, mid-range of Mucci's 20-40 bar fit window (Table 2)
  double meoh_demand_kg_h2_per_s = 0.0; // constant downstream H2 demand -- caller-supplied, no fabricated default
  double dt_s = 3600.0;   // timestep duration; 3600 s (hourly) matches typical day-ahead price granularity
  StorageConfig storage;
};

struct DispatchTimestepResult {
  double price_USD_per_MWh = 0.0;
  bool   electrolyzer_on = false;
  front_end::ElectrolyzerResult electrolyzer; // full result from Chapter 6.1's model this step (zeroed if off)
  StorageStepResult storage_step;
  double electricity_cost_USD = 0.0; // electrolyzer.P_total_MW * (dt_s/3600) * price
};

struct DispatchRunResult {
  std::vector<DispatchTimestepResult> steps;
  double total_electricity_cost_USD = 0.0;
  double total_h2_produced_kg = 0.0;
  double total_h2_consumed_kg = 0.0;
  int    n_floor_violations = 0; // count of timesteps where storage could not cover meoh_demand -- see storage.hpp

  // Count of timesteps where the vessel hit p_max_bar and could not absorb all the hydrogen produced, plus the total surplus
  // A schedule with a non-zero count is infeasible as written: a real plant would have had to curtail the electrolyzer or vent
  // Reported separately from the floor so an undersized vessel and an over-eager production rule are distinguishable
  int    n_ceiling_violations = 0;
  double total_h2_vented_kg = 0.0;
  int    n_steps_on = 0;
  StorageState final_state;
  bool   ok = true;
  std::string message;
};

// Runs the price-threshold rule over the full price series, one timestep per entry
// Using dt_s from cfg for every step 
// (a caller with irregular timestamps should pre-resample the series this function does not infer timestep duration from the data)
DispatchRunResult run_price_threshold_dispatch(const std::vector<double>& price_USD_per_MWh_series,
                                                const StorageState& initial_state,
                                                const DispatchConfig& cfg);

// Presets
namespace presets {

// Converts Lim's own electricity price convention (USD/kWh, capex_opex.hpp
// ReactantPrices::electricity_USD_per_kWh) into this module's USD/MWh unit,
// a pure unit-conversion helper, not a new price source
inline double usd_per_kwh_to_usd_per_mwh(double usd_per_kwh) { return usd_per_kwh * 1000.0; }

}

}
