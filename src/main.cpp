// Demo driver

// Runs the dispatch, reactor and economics pipeline end to end on an illustrative price series and prints a summary

// The price series here is synthetic and is only meant to exercise the dispatch logic
// It is not market data and no conclusion should be drawn

#include <cstdio>
#include <vector>

#include "flowsheet/hybrid_plant_design_point_full.hpp"
#include "sampling/feasibility.hpp"
#include "sampling/fingerprint.hpp"
#include "sampling/validity.hpp"

int main() {
  using namespace flowsheet;

  // Two days, hourly: cheap overnight, expensive during the day
  std::vector<double> prices;
  for (int h = 0; h < 48; ++h) {
    const int hour_of_day = h % 24;
    prices.push_back(hour_of_day < 8 ? 12.0 : 55.0);
  }

  HybridPlantDesignPointFullConfig cfg;
  cfg.base_cfg.co2_feed_kg_s = presets::van_dal_industrial_co2_kg_s();
  cfg.base_cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.base_cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.base_cfg.dispatch_cfg.storage.V_m3 = 5000.0;
  cfg.base_cfg.dispatch_cfg.meoh_demand_kg_h2_per_s = 0.01;

  dispatch::StorageState initial;
  initial.M_H2_kg = 1000.0;

  const auto res = evaluate_design_point_full(prices, initial, cfg);

  std::printf("methanol_twin -- design point summary\n");
  std::printf("=====================================\n\n");
  if (!res.ok) {
    std::printf("run failed: %s\n", res.message.c_str());
    return 1;
  }

  const auto& b = res.base;
  std::printf("Reactor\n");
  std::printf("  tubes                    %d\n", b.bed_used.n_tubes);
  std::printf("  catalyst charge          %.0f kg\n",
              reactor::catalyst_mass_total_kg(b.bed_used));
  std::printf("  activity                 %.3f\n", b.activity_used);
  std::printf("  overall CO2 conversion   %.2f %%\n", 100.0 * b.co2_conversion_overall);
  std::printf("  recycle passes           %d\n", b.reactor_chain.recycle.n_iterations);

  std::printf("\nProduction\n");
  std::printf("  methanol product         %.4f kg/s\n", b.meoh_product_kg_s);
  std::printf("  annual production        %.0f t/y\n", b.annual_production_t);

  std::printf("\nPower\n");
  std::printf("  compression              %.2f MW\n", b.compression.total_power_MW);
  std::printf("  recycle circulator       %.2f MW\n",
              b.reactor_chain.recycle.circulator.P_comp_MW);
  std::printf("  effluent cooling load    %.1f MW  (total, reactor to drum)\n",
              b.reactor_chain.recycle.effluent_cooling_duty_MW);

  std::printf("\nHeat integration (Van-Dal HX4)\n");
  std::printf("  feed pre-heat required   %.1f MW\n",
              b.reactor_chain.recycle.feed_preheat_duty_MW);
  std::printf("  recovered by the FEHE    %.1f MW  (effectiveness %.3f)\n",
              b.reactor_chain.recycle.fehe.duty_MW,
              b.reactor_chain.recycle.fehe.effectiveness);
  std::printf("  trim heater              %.1f MW\n",
              b.reactor_chain.recycle.trim_heater_duty_MW);
  std::printf("  trim cooler              %.1f MW\n",
              b.reactor_chain.recycle.trim_cooler_duty_MW);

  std::printf("\nEquipment sizing\n");
  std::printf("  knockout drum            %.2f m dia x %.2f m\n",
              res.knockout_drum_sizing.diameter_m, res.knockout_drum_sizing.length_m);
  std::printf("  distillation column      %.2f m dia\n", res.column_sizing.diameter_m);
  std::printf("  column top / bottom      %.1f / %.1f K\n",
              res.column_sizing.top_bubble_point.T_K,
              res.column_sizing.bottom_bubble_point.T_K);

  std::printf("\nEconomics (CEPCI %d)\n", b.capex.cepci_target_year);
  std::printf("  CAPEX, reactor+WE+comp   $%.4g\n", b.capex.total_module_cost_escalated);
  std::printf("  CAPEX, incl. vessels     $%.4g\n", res.capex_full.total_module_cost_escalated);
  std::printf("  annual OPEX              $%.4g /y\n", b.annual_opex.total_opex_USD_per_y);
  std::printf("  unit cost, base scope    $%.1f /t\n", res.unit_cost_base_USD_per_t);
  std::printf("  unit cost, full scope    $%.1f /t\n", res.unit_cost_full_USD_per_t);

  const auto cls = feasibility::classify(b);
  std::printf("\nFeasibility                %s\n", feasibility::to_string(cls.outcome));

  const auto vr = validity::check_design_point(cfg.base_cfg, b);
  std::printf("\nValidity excursions        %zu\n", vr.excursions.size());
  for (const auto& e : vr.excursions) {
    std::printf("  %-46s %.4g (valid %.4g to %.4g)\n", e.quantity, e.value, e.lo, e.hi);
  }

  std::printf("\nModel fingerprint          %s\n", fingerprint::hex_hash().c_str());
  std::printf("\nThe price series above is synthetic and illustrative only.\n");
  return 0;
}
