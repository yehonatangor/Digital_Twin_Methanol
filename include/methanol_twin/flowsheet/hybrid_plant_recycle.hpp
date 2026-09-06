#pragma once
// Top-level composition, recycle-loop variant of hybrid_plant.hpp
//
// Composes rather than edits: the dispatch loop, the reactor chain and the
// economics trio are the same calls, with run_co2_h2_plant_with_recycle()
// substituted for the single-pass run_co2_h2_plant()
//
// One number must change. hybrid_plant.cpp charges OPEX for cfg.co2_feed_kg_s
// because single-pass makes "CO2 fed" and "CO2 purchased" the same figure
// With recycle they are not, so co2_kg_s is taken from the fresh design feed
// and never from reactor_chain.converged_reactor_inlet, which is larger by the
// recycle ratio
//
// Scope is inherited: CAPEX covers the reactor on the shell-and-tube
// convention and the electrolyzer on Lim's $/kW, with no compressor or vessel
// costing. See docs/23-flowsheet-composition.md

#include <string>
#include <vector>

#include "dispatch/dispatch_loop.hpp"
#include "flowsheet/co2_h2_plant_recycle.hpp"
#include "economics/plant_economics.hpp"

namespace flowsheet {

struct HybridPlantRecycleConfig {
  dispatch::DispatchConfig dispatch_cfg;
  RecycleLoopConfig recycle_cfg;   // carries plant_cfg and the recycle fraction, default 0.99

  double co2_feed_kg_s = 0.0;   // FRESH, steady-state design CO2 feed -- no default, caller's own design point (same contract as hybrid_plant.hpp)

  // Reactor-as-shell-and-tube costing inputs -- same fields, same "caller's
  // own choice" contract as hybrid_plant.hpp
  economics::MaterialOfConstruction reactor_moc = economics::MaterialOfConstruction::CarbonSteel;
  double reactor_FM = 1.0;
  double reactor_FP = 1.0;

  // Lim Table 2, PEMEL 2020
  double electrolyzer_usd_per_kW = 1188.0;
  double we_stack_lifetime_years = 8.0;
  int    project_years = 20;

  int    cepci_target_year = 2016;
  economics::ReactantPrices reactant_prices;
};

struct HybridPlantRecycleResult {
  dispatch::DispatchRunResult dispatch;
  RecycleLoopResult reactor_chain;               // converged recycle loop, not a single pass

  economics::PlantCapexResult capex;
  economics::PlantOpexResult annual_opex;
  economics::PlantEconomicsResult plant_economics;

  double dispatch_avg_electricity_MW = 0.0;

  bool ok = false;
  std::string message;
};

HybridPlantRecycleResult run_hybrid_plant_recycle(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecycleConfig& cfg = HybridPlantRecycleConfig{});

}  // namespace flowsheet
