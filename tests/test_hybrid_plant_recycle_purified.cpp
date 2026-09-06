// The same composition, but production is the distillate

#include "flowsheet/hybrid_plant_recycle_purified.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== hybrid plant, recycle + purification ===\n\n");

  flowsheet::HybridPlantRecyclePurifiedConfig cfg;
  cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::run_hybrid_plant_recycle_purified(demoPrices(), demoStorage(), cfg);

  std::printf("[1] It runs\n");
  checkTrue("ok", r.ok);
  checkTrue("the loop converged", r.reactor_chain.recycle.converged);
  checkTrue("the column ran", r.reactor_chain.distillation.ok);

  std::printf("\n[2] Annual production is based on the DISTILLATE\n");
  const double op_s = 365.0 * 24.0 * 3600.0 * cfg.reactant_prices.stream_factor;
  checkRel("production tracks the distillate mass flow",
           r.plant_economics.annual_production_t,
           r.reactor_chain.meoh_product_kg_s * op_s / 1000.0, 1e-9);
  checkTrue("which is at most the crude figure",
            r.reactor_chain.meoh_product_kg_s <=
            r.reactor_chain.recycle.meoh_production_kg_s + 1e-9);

  std::printf("\n[3] Purity is real, so the tonnes are sellable tonnes\n");
  checkTrue("purity target met",
            r.reactor_chain.distillation.actual_purity_wt_fraction >= 0.999 - 1e-6);
  std::printf("       [INFO] %.0f t/y at $%.1f/t\n",
              r.plant_economics.annual_production_t, r.plant_economics.unit_cost_USD_per_t);

  std::printf("\n[4] The column's own capital cost is still declared missing\n");
  checkTrue("the message says the column is not costed here",
            r.message.find("column") != std::string::npos);

  return report("hybrid plant recycle purified");
}
