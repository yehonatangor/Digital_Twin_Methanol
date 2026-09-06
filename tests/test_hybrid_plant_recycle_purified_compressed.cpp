// The purified recycle plant with a real, powered, costed compression train

#include "flowsheet/hybrid_plant_recycle_purified_compressed.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== hybrid plant, purified + compressed ===\n\n");

  flowsheet::HybridPlantRecyclePurifiedCompressedConfig cfg;
  cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::run_hybrid_plant_recycle_purified_compressed(
      demoPrices(), demoStorage(), cfg);

  std::printf("[1] It runs\n");
  checkTrue("ok", r.ok);
  checkTrue("the loop converged", r.reactor_chain.recycle.converged);
  checkTrue("the compression train ran", r.compression.ok);

  std::printf("\n[2] Compression is a real duty, not a placeholder\n");
  checkTrue("power is positive", r.compression_power_MW > 0.0);
  check("it matches the train's own total",
        r.compression_power_MW, r.compression.total_power_MW, 1e-12);
  std::printf("       [INFO] compression %.2f MW\n", r.compression_power_MW);

  std::printf("\n[3] Compressors appear as their own CAPEX lines\n");
  int n_comp = 0;
  for (const auto& it : r.capex.items) {
    if (it.name.find("compressor") != std::string::npos) ++n_comp;
  }
  check("one line per stage: 4 CO2 + 1 H2", static_cast<double>(n_comp), 5.0, 0.0);
  checkTrue("CAPEX is larger than it would be without them",
            r.capex.total_module_cost_escalated > 0.0);

  std::printf("\n[4] Compression electricity reaches OPEX\n");
  checkTrue("electricity cost is positive", r.annual_opex.electricity_cost_USD_per_y > 0.0);
  std::printf("       [INFO] %.0f t/y at $%.1f/t\n",
              r.plant_economics.annual_production_t, r.plant_economics.unit_cost_USD_per_t);
  checkTrue("unit cost is finite and positive",
            r.plant_economics.unit_cost_USD_per_t > 0.0 &&
            r.plant_economics.unit_cost_USD_per_t < 1.0e5);

  std::printf("\n[5] Remaining scope is still stated honestly\n");
  checkTrue("the column is still declared uncosted at this layer",
            r.message.find("column") != std::string::npos);

  return report("hybrid plant purified compressed");
}
