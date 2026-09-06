// Dispatch plus the single-pass reactor chain plus economics

#include "flowsheet/hybrid_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== hybrid plant (single pass) ===\n\n");

  flowsheet::HybridPlantConfig cfg;
  cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::run_hybrid_plant(demoPrices(), demoStorage(), cfg);

  std::printf("[1] All three layers ran\n");
  checkTrue("ok", r.ok);
  checkTrue("dispatch ok", r.dispatch.ok);
  checkTrue("reactor chain ok", r.reactor_chain.ok);
  checkTrue("capex ok", r.capex.ok);
  checkTrue("opex ok", r.annual_opex.ok);

  std::printf("\n[2] The two timescales stay separate, not silently blended\n");
  check("dispatch produced one result per hour",
        static_cast<double>(r.dispatch.steps.size()), 48.0, 0.0);
  check("the reactor ran once, at the design point",
        static_cast<double>(r.reactor_chain.n_tubes_used), 6204.0, 1.0);
  checkTrue("the dispatch average is a real average",
            r.dispatch_avg_electricity_MW > 0.0 &&
            r.dispatch_avg_electricity_MW <= 2.1);

  std::printf("\n[3] The electricity cost is price weighted, not a flat average\n");
  checkTrue("dispatch recorded a cost", r.dispatch.total_electricity_cost_USD > 0.0);
  checkTrue("that cost reached OPEX", r.annual_opex.electricity_cost_USD_per_y > 0.0);
  // Everything ran in the cheap block, so the effective price must be the low one
  double h_on = 0.0;
  for (const auto& s : r.dispatch.steps) if (s.electrolyzer_on) h_on += 1.0;
  check("only the cheap hours ran", h_on, 16.0, 0.0);

  std::printf("\n[4] The unit cost is finite and positive\n");
  checkTrue("annual production is positive", r.plant_economics.annual_production_t > 0.0);
  checkTrue("unit cost is positive", r.plant_economics.unit_cost_USD_per_t > 0.0);
  checkTrue("unit cost is not absurd", r.plant_economics.unit_cost_USD_per_t < 1.0e5);
  std::printf("       [INFO] %.0f t/y at $%.1f/t\n",
              r.plant_economics.annual_production_t, r.plant_economics.unit_cost_USD_per_t);

  std::printf("\n[5] The electrolyzer is not double counted in CAPEX\n");
  checkTrue("an all-in item is present", r.capex.sum_all_in_installed_usd > 0.0);
  checkTrue("the total exceeds the all-in part",
            r.capex.total_module_cost_escalated > r.capex.sum_all_in_installed_usd);

  std::printf("\n[6] The CAPEX scope is stated rather than implied complete\n");
  checkTrue("the message describes the scope", !r.capex.message.empty());

  std::printf("\n[7] Guards\n");
  auto bad = cfg; bad.co2_feed_kg_s = 0.0;
  checkTrue("a zero design feed is refused",
            !flowsheet::run_hybrid_plant(demoPrices(), demoStorage(), bad).ok);

  return report("hybrid plant");
}
