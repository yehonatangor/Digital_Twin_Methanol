// The design point with the knockout drum and the distillation column costed

#include "flowsheet/hybrid_plant_design_point_full.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== design point, full costing ===\n\n");

  flowsheet::HybridPlantDesignPointFullConfig cfg;
  cfg.base_cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.base_cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.base_cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.base_cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::evaluate_design_point_full(demoPrices(), demoStorage(), cfg);

  std::printf("[1] It runs and the base result is untouched\n");
  checkTrue("ok", r.ok);
  checkTrue("base ok", r.base.ok);
  check("base unit cost is carried through",
        r.unit_cost_base_USD_per_t, r.base.unit_cost_USD_per_t, 1e-12);

  std::printf("\n[2] The knockout drum is sized from the real vapour stream\n");
  checkTrue("drum sizing ok", r.knockout_drum_sizing.ok);
  checkTrue("diameter is physical",
            r.knockout_drum_sizing.diameter_m > 0.1 &&
            r.knockout_drum_sizing.diameter_m < 30.0);
  check("length follows the Table 23.10 ratio at loop pressure",
        r.knockout_drum_sizing.length_m,
        r.knockout_drum_sizing.diameter_m * 5.0, 0.01);

  std::printf("\n[3] The column is sized from real bubble points\n");
  checkTrue("column sizing ok", r.column_sizing.ok);
  checkTrue("top bubble point converged",    r.column_sizing.top_bubble_point.converged);
  checkTrue("bottom bubble point converged", r.column_sizing.bottom_bubble_point.converged);
  // At 1.2 bar this project's own DIPPR-101 correlations give pure methanol
  // 342.05 K and pure water 377.98 K, so the two column ends must bracket
  // toward those values with the distillate end the cooler one
  //
  // The bottoms figure is a regression guard. It previously sat near 360 K,
  // roughly 17 K below pure water, because dissolved carbon dioxide and
  // hydrogen were being routed into the liquid bottoms and depressing its
  // bubble point. Those permanent gases now leave as a vent, so the bottoms
  // is condensables only and boils where a nearly pure water stream should
  checkTrue("the distillate end is cooler than the bottoms",
            r.column_sizing.top_bubble_point.T_K < r.column_sizing.bottom_bubble_point.T_K);
  check("methanol-rich top, K",  r.column_sizing.top_bubble_point.T_K,    342.05, 3.0);
  check("water-rich bottoms, K", r.column_sizing.bottom_bubble_point.T_K, 377.5,  3.0);
  checkTrue("the bottoms sits at or just below pure water at 1.2 bar",
            r.column_sizing.bottom_bubble_point.T_K <= 377.98 + 1e-6);
  checkTrue("diameter is physical",
            r.column_sizing.diameter_m > 0.1 && r.column_sizing.diameter_m < 30.0);
  check("the column is sized for the limiting section",
        r.column_sizing.diameter_m,
        std::max(r.column_sizing.top_section.diameter_m,
                 r.column_sizing.bottom_section.diameter_m), 1e-12);

  std::printf("\n[4] Three new CAPEX lines appear, and they cost something\n");
  checkTrue("drum item is priced",   r.knockout_drum_item.CBM_2001usd > 0.0);
  checkTrue("shell item is priced",  r.column_shell_item.CBM_2001usd  > 0.0);
  checkTrue("trays item is priced",  r.column_trays_item.CBM_2001usd  > 0.0);
  check("three items were added",
        static_cast<double>(r.capex_full.items.size()),
        static_cast<double>(r.base.capex.items.size()) + 3.0, 0.0);

  std::printf("\n[5] Closing the gap raises CAPEX and the unit cost\n");
  checkTrue("full CAPEX exceeds base CAPEX",
            r.capex_full.total_module_cost_escalated >
            r.base.capex.total_module_cost_escalated);
  checkTrue("full unit cost exceeds base unit cost",
            r.unit_cost_full_USD_per_t > r.unit_cost_base_USD_per_t);
  std::printf("       [INFO] $%.1f/t base, $%.1f/t with vessels and column\n",
              r.unit_cost_base_USD_per_t, r.unit_cost_full_USD_per_t);

  std::printf("\n[6] Production is unchanged; only the cost basis grew\n");
  check("same annual production",
        r.plant_economics_full.annual_production_t, r.base.annual_production_t, 1e-9);

  std::printf("\n[7] The message states that the gap is now closed\n");
  checkTrue("it says so", r.message.find("closed") != std::string::npos);

  return report("design point full");
}
