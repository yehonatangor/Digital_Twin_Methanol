#pragma once
// =============================================================================
// hybrid_plant_recycle_purified.hpp -- top-level composition, PURIFIED
// product-stream variant
// =============================================================================
// WHY THIS IS A SEPARATE FILE FROM hybrid_plant_recycle.hpp, NOT AN EDIT TO
// IT. Same discipline as every prior integration layer in this project:
// hybrid_plant_recycle.hpp (Chapter 8.5) is already built, tested,
// documented. This file reuses its own composed pieces --
// dispatch::run_price_threshold_dispatch(), the SAME economics::
// aggregate_capex()/compute_opex()/run_plant_economics() trio -- and swaps
// only the reactor-chain step for flowsheet::run_co2_h2_plant_recycle_
// purified() (this chapter), which itself is co2_h2_plant_recycle.hpp's own
// entry point plus Chapter 6.9's distillation, composed, not edited
//
// THE ONE NUMBER THAT MUST CHANGE, AND WHY. hybrid_plant_recycle.cpp computes
// annual_production_t from reactor_chain.meoh_production_kg_s -- the crude,
// CH3OH-mass-only figure co2_h2_plant_recycle.hpp's own header already flags
// as NOT accounting for separation recovery or purity. This module uses
// reactor_chain.meoh_product_kg_s instead -- the actual DISTILLATE mass flow,
// Shi's own 99.5% recovery / 99.9 wt% purity already applied -- as the basis
// for annual production. This is the only deliberate change from hybrid_
// plant_recycle.cpp's own wiring; OPEX's fresh-CO2 basis, CAPEX, WE
// replacement, and dispatch wiring are all copied unchanged
//
// SCOPE, CARRIED OVER UNCHANGED. CAPEX still covers only the reactor
// (shell-and-tube convention) and the electrolyzer -- no compressor, no
// vessel/column costing. In particular, THE DISTILLATION COLUMN ITSELF IS
// STILL NOT COSTED (co2_h2_plant_recycle_purified.hpp's own header explains
// why: no sourced column-sizing method exists in this project's library)
// So this module closes the "is the reported production a real, sellable
// product" gap while leaving the "is the column's own capital cost in the
// number" gap exactly as open as it was before -- both stated in
// HybridPlantRecyclePurifiedResult::message, not silently implied closed
// =============================================================================

#include <string>
#include <vector>

#include "dispatch/dispatch_loop.hpp"
#include "flowsheet/co2_h2_plant_recycle_purified.hpp"
#include "economics/plant_economics.hpp"

namespace flowsheet {

struct HybridPlantRecyclePurifiedConfig {
  dispatch::DispatchConfig dispatch_cfg;
  PurifiedRecycleLoopConfig purified_cfg;   // recycle_cfg (reactor T/P/bed/purge) + distillation_cfg (Shi defaults)

  double co2_feed_kg_s = 0.0;   // FRESH, steady-state design CO2 feed -- same contract as hybrid_plant_recycle.hpp

  economics::MaterialOfConstruction reactor_moc = economics::MaterialOfConstruction::CarbonSteel;
  double reactor_FM = 1.0;
  double reactor_FP = 1.0;

  double electrolyzer_usd_per_kW = 1188.0;
  double we_stack_lifetime_years = 8.0;
  int    project_years = 20;

  int    cepci_target_year = 2016;
  economics::ReactantPrices reactant_prices;
};

struct HybridPlantRecyclePurifiedResult {
  dispatch::DispatchRunResult dispatch;
  PurifiedRecycleLoopResult reactor_chain;       // recycle loop plus distillation

  economics::PlantCapexResult capex;
  economics::PlantOpexResult annual_opex;
  economics::PlantEconomicsResult plant_economics;

  double dispatch_avg_electricity_MW = 0.0;

  bool ok = false;
  std::string message;   // states the column-CAPEX gap explicitly, see header
};

HybridPlantRecyclePurifiedResult run_hybrid_plant_recycle_purified(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecyclePurifiedConfig& cfg = HybridPlantRecyclePurifiedConfig{});

}  // namespace flowsheet
