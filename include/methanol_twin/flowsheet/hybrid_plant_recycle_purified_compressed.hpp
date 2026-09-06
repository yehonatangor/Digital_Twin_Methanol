#pragma once
// =============================================================================
// hybrid_plant_recycle_purified_compressed.hpp -- top-level composition,
// WITH a real, costed, powered compression train
// =============================================================================
// WHY THIS IS A SEPARATE FILE FROM hybrid_plant_recycle_purified.hpp, NOT AN
// EDIT TO IT. Same discipline as every prior integration layer: hybrid_
// plant_recycle_purified.hpp (Chapter 8.6) is already built, tested,
// documented. This file reuses its own composed pieces -- dispatch::
// run_price_threshold_dispatch(), flowsheet::run_co2_h2_plant_recycle_
// purified(), the SAME economics::aggregate_capex()/compute_opex()/
// run_plant_economics() trio -- and adds exactly ONE new piece on top:
// flowsheet::run_compression_train() (this chapter), which itself composes
// front_end::compressor_train.hpp (also this chapter) rather than editing
// anything
//
// TWO DELIBERATE CHANGES FROM hybrid_plant_recycle_purified.cpp's OWN
// WIRING, BOTH STATED IN compression_train.hpp'S HEADER AND CONFIRMED WITH
// THE USER BEFORE BUILDING:
//   1. CAPEX gains new line items: one per CO2 compression stage (4, by
//      compression_train.hpp's own default) plus one per H2 parallel unit
//      (2, Mucci's own stated practice) -- each costed via economics::
//      cost_compressor(), the SAME Turton "remaining equipment" path
//      already used nowhere else in this project's CAPEX (compressor.hpp's
//      own model was built but never wired into a CAPEX total before now)
//   2. OPEX's electricity_MW gains the compression train's total power,
//      added on top of the dispatch loop's own average electrolyzer draw
//      -- STEADY, not price-dispatched (confirmed with the user: the
//      reactor needs continuous feed gas regardless of electricity price,
//      so compression runs whenever the plant runs)
//
// SCOPE, CARRIED OVER UNCHANGED. The distillation column's own CAPEX/duty
// is STILL not costed (Chapter 8.6's own stated gap, unaffected by this
// chapter). The CO2 compressor's efficiency numbers are an extension of
// Mucci's H2-validated model, with a stated real-gas caveat near CO2's
// critical point (compressor_train.hpp's own header) -- this is a REAL,
// costed number now, not a placeholder, but it carries a documented
// confidence caveat H2's own compressor number does not
// =============================================================================

#include <string>
#include <vector>

#include "dispatch/dispatch_loop.hpp"
#include "flowsheet/co2_h2_plant_recycle_purified.hpp"
#include "flowsheet/compression_train.hpp"
#include "economics/plant_economics.hpp"

namespace flowsheet {

struct HybridPlantRecyclePurifiedCompressedConfig {
  dispatch::DispatchConfig dispatch_cfg;
  PurifiedRecycleLoopConfig purified_cfg;
  CompressionTrainConfig compression_cfg;

  double co2_feed_kg_s = 0.0;   // FRESH, steady-state design CO2 feed

  economics::MaterialOfConstruction reactor_moc = economics::MaterialOfConstruction::CarbonSteel;
  double reactor_FM = 1.0;
  double reactor_FP = 1.0;
  double electrolyzer_usd_per_kW = 1188.0;
  double we_stack_lifetime_years = 8.0;
  int    project_years = 20;

  int    cepci_target_year = 2016;
  economics::ReactantPrices reactant_prices;
};

struct HybridPlantRecyclePurifiedCompressedResult {
  dispatch::DispatchRunResult dispatch;
  PurifiedRecycleLoopResult reactor_chain;
  CompressionTrainResult compression;            // real, costed, powered train

  economics::PlantCapexResult capex;             // reactor + electrolyzer + compressors
  economics::PlantOpexResult annual_opex;        // includes compression electricity
  economics::PlantEconomicsResult plant_economics;

  double dispatch_avg_electricity_MW = 0.0;
  double compression_power_MW = 0.0;             // steady-state design-point duty

  bool ok = false;
  std::string message;
};

HybridPlantRecyclePurifiedCompressedResult run_hybrid_plant_recycle_purified_compressed(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecyclePurifiedCompressedConfig& cfg =
        HybridPlantRecyclePurifiedCompressedConfig{});

}  // namespace flowsheet
