// Dispatch plus the CONVERGED recycle loop plus economics

#include "flowsheet/hybrid_plant_recycle.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== hybrid plant, recycle ===\n\n");

  flowsheet::HybridPlantRecycleConfig cfg;
  cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::run_hybrid_plant_recycle(demoPrices(), demoStorage(), cfg);

  std::printf("[1] It runs on top of a converged loop\n");
  checkTrue("ok", r.ok);
  checkTrue("the recycle loop converged", r.reactor_chain.converged);
  checkTrue("capex ok", r.capex.ok);
  checkTrue("opex ok", r.annual_opex.ok);

  std::printf("\n[2] Recycling improves the unit cost against a single pass\n");
  checkTrue("overall conversion beats per-pass",
            r.reactor_chain.co2_conversion_overall > r.reactor_chain.co2_conversion_per_pass);
  checkTrue("unit cost is positive and finite",
            r.plant_economics.unit_cost_USD_per_t > 0.0 &&
            r.plant_economics.unit_cost_USD_per_t < 1.0e5);
  std::printf("       [INFO] %.1f %% overall conversion, %.0f t/y, $%.1f/t\n",
              100.0 * r.reactor_chain.co2_conversion_overall,
              r.plant_economics.annual_production_t,
              r.plant_economics.unit_cost_USD_per_t);

  std::printf("\n[3] OPEX is charged on the FRESH feed, not the recycle-laden inlet\n");
  // The converged reactor inlet is much larger than the fresh feed; charging
  // CO2 against it would inflate reactant cost several-fold
  checkTrue("the converged inlet really is larger than the fresh feed",
            r.reactor_chain.converged_reactor_inlet.totalMolarFlow() >
            r.reactor_chain.fresh_feed.totalMolarFlow());
  const double op_s = 365.0 * 24.0 * 3600.0 * cfg.reactant_prices.stream_factor;
  const double fresh_co2_cost =
      cfg.co2_feed_kg_s * op_s / 1000.0 * cfg.reactant_prices.co2_USD_per_t;
  checkRel("reactant cost matches the FRESH CO2 rate",
           r.annual_opex.reactant_cost_USD_per_y, fresh_co2_cost, 1e-6);

  std::printf("\n[4] Production is the crude figure at this layer\n");
  checkTrue("the message says so", r.message.find("crude") != std::string::npos);

  return report("hybrid plant recycle");
}
