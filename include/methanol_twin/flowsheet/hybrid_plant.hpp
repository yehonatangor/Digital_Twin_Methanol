#pragma once
// Dispatch plus reactor chain plus economics
//
// Pure wiring: calls run_price_threshold_dispatch, run_co2_h2_plant and the
// economics aggregation, passing real results between them. No new physics or
// economics formulas
// See docs/23-flowsheet-composition.md

#include <string>
#include <vector>

#include "dispatch/dispatch_loop.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "economics/plant_economics.hpp"

namespace flowsheet {

struct HybridPlantConfig {
  dispatch::DispatchConfig dispatch_cfg;
  Co2H2PlantConfig reactor_chain_cfg;

  double co2_feed_kg_s = 0.0;   // steady-state design CO2 feed for the reactor chain -- no default, caller's own design point

  // Reactor-as-shell-and-tube costing inputs (Turton Eq. A.3/A.4 -- no
  // sourced default for either, same "caller's own choice" contract as
  // capex_opex.hpp's PressureVesselCostConfig::FM/FP)
  economics::MaterialOfConstruction reactor_moc = economics::MaterialOfConstruction::CarbonSteel;
  double reactor_FM = 1.0;
  double reactor_FP = 1.0;

  // Lim Table 2, PEMEL 2020 default (presets::lim_electrolyzer_capex_PEMEL()[0])
  double electrolyzer_usd_per_kW = 1188.0;
  double we_stack_lifetime_years = 8.0;    // Lim Table 2, PEMEL default
  int    project_years = 20;               // Lim Sec. 2.2.1 default

  int    cepci_target_year = 2016;         // latest year Table 7.4 (capex_opex.hpp) prints
  economics::ReactantPrices reactant_prices;  // Lim Table 1 defaults
};

struct HybridPlantResult {
  dispatch::DispatchRunResult dispatch;         // time-resolved: exact electricity cost, H2 schedule, storage state
  Co2H2PlantResult reactor_chain;                // steady-state design point: conversion, methanol yield

  economics::PlantCapexResult capex;             // reactor + electrolyzer, Turton/Lim (see header scope note)
  economics::PlantOpexResult annual_opex;        // steady-state annual estimate, compute_opex() as in Chapter 7.4
  economics::PlantEconomicsResult plant_economics;  // CRF-annualized CAPEX + annual_opex -> unit cost (Lim Eq. 6)

  double dispatch_avg_electricity_MW = 0.0;      // time-averaged P_total_MW over the whole dispatch run (incl. idle steps)

  bool ok = false;
  std::string message;
};

HybridPlantResult run_hybrid_plant(const std::vector<double>& price_USD_per_MWh_series,
                                    const dispatch::StorageState& initial_storage_state,
                                    const HybridPlantConfig& cfg);

}  // namespace flowsheet
