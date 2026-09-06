#pragma once
// One call: geometry and activity in, full plant economics out. The entry
// point a parameter sweep or an optimizer is meant to call
//
// Pure wiring. Bed geometry and catalyst activity are promoted to top-level
// fields, the aged purified recycle loop plus compression train is run at a
// steady-state design point, and dispatch supplies the electricity cost
// Every number comes from a function this project already validates
//
// Activity is a genuine input: it scales the LHHW rate through
// integration::integrate_aged_reactor(), so it moves conversion, production
// and unit cost, rather than being a cost line added afterwards

#include <string>
#include <vector>

#include "dispatch/dispatch_loop.hpp"
#include "flowsheet/co2_h2_plant_recycle_purified.hpp"
#include "flowsheet/compression_train.hpp"
#include "economics/plant_economics.hpp"
#include "reactor/reactor_core.hpp"

namespace flowsheet {

struct HybridPlantDesignPointConfig {
  // The two swept variables, promoted to the top level
  reactor::BedGeometry bed;          // n_tubes <= 1 selects the Van-Dal/Shi composite bed
  double activity = 1.0;             // (0, 1], 1.0 being fresh catalyst

  double co2_feed_kg_s = 0.0;        // fresh, steady-state design feed
  double recycle_fraction = 0.70;    // see co2_h2_plant_recycle.hpp on the flux wall

  double reactor_inlet_T_K = 483.15;
  double reactor_inlet_P_bar = 78.0;

  dispatch::DispatchConfig dispatch_cfg;
  CompressionTrainConfig compression_cfg;
  front_end::DistillationConfig distillation_cfg;

  economics::MaterialOfConstruction reactor_moc = economics::MaterialOfConstruction::CarbonSteel;
  double reactor_FM = 1.0;
  double reactor_FP = 1.0;

  double electrolyzer_usd_per_kW = 1188.0;   // Lim Table 2, PEMEL 2020
  double we_stack_lifetime_years = 8.0;
  int    project_years = 20;
  int    cepci_target_year = 2016;
  economics::ReactantPrices reactant_prices;
};

struct HybridPlantDesignPointResult {
  dispatch::DispatchRunResult dispatch;
  PurifiedRecycleLoopResult reactor_chain;
  CompressionTrainResult compression;

  economics::PlantCapexResult capex;
  economics::PlantOpexResult annual_opex;
  economics::PlantEconomicsResult plant_economics;

  reactor::BedGeometry bed_used;          // resolved bed, after the preset default
  double activity_used = 1.0;

  double co2_conversion_overall = 0.0;
  double meoh_product_kg_s = 0.0;         // distillate, not crude
  double annual_production_t = 0.0;
  double unit_cost_USD_per_t = 0.0;
  double total_electricity_MW = 0.0;      // compression plus dispatch average

  bool ok = false;
  std::string message;
};

HybridPlantDesignPointResult evaluate_design_point(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantDesignPointConfig& cfg = HybridPlantDesignPointConfig{});

}  // namespace flowsheet
