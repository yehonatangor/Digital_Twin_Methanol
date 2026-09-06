// The compressed purified plant, plus an activity trajectory and a catalyst
// replacement OPEX line

#include "flowsheet/hybrid_plant_recycle_purified_compressed_aged.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"
#include "_hybrid_common.inc"

int main() {
  std::printf("=== hybrid plant, aged ===\n\n");

  flowsheet::HybridPlantRecyclePurifiedCompressedAgedConfig cfg;
  cfg.base_cfg.co2_feed_kg_s = flowsheet::presets::van_dal_industrial_co2_kg_s();
  cfg.base_cfg.dispatch_cfg.electrolyzer_power_MW = 2.0;
  cfg.base_cfg.dispatch_cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.base_cfg.dispatch_cfg.storage.V_m3 = 5000.0;

  const auto r = flowsheet::run_hybrid_plant_recycle_purified_compressed_aged(
      demoPrices(), demoStorage(), cfg);

  std::printf("[1] The base result is carried through unchanged\n");
  checkTrue("ok", r.ok);
  checkTrue("base ok", r.base.ok);
  checkTrue("the loop converged", r.base.reactor_chain.recycle.converged);

  std::printf("\n[2] The activity trajectory is informational and in range\n");
  checkTrue("trajectory ok", r.activity_trajectory.ok);
  checkTrue("it starts fresh and decays",
            r.activity_trajectory.profile.front().a >= r.activity_trajectory.a_final);
  checkTrue("final activity is a fraction",
            r.activity_trajectory.a_final > 0.0 && r.activity_trajectory.a_final <= 1.0);
  checkTrue("the default horizon stays inside Fichtl's fitted range",
            r.trajectory_within_fichtl_fitted_range);
  checkTrue("the aging temperature is the reactor's own hot end",
            r.aging_temperature_K > 400.0);
  std::printf("       [INFO] a(%.0f h) = %.4f at %.1f K\n",
              cfg.aging_display_cfg.t_end_h, r.activity_trajectory.a_final,
              r.aging_temperature_K);

  std::printf("\n[3] Running past the fitted range is flagged, not hidden\n");
  auto longer = cfg;
  longer.aging_display_cfg.t_end_h = 5000.0;
  const auto rl = flowsheet::run_hybrid_plant_recycle_purified_compressed_aged(
      demoPrices(), demoStorage(), longer);
  checkTrue("still runs", rl.ok);
  checkTrue("but is flagged as extrapolated", !rl.trajectory_within_fichtl_fitted_range);
  checkTrue("and says so in the message",
            rl.message.find("extrapolated") != std::string::npos);

  std::printf("\n[4] Catalyst replacement adds a real OPEX line\n");
  check("catalyst mass is the bed charge, kg", r.catalyst_mass_total_kg, 44500.0, 5.0);
  checkTrue("replacements were counted", r.catalyst_replacement.n_replacements > 0);
  checkTrue("the annualized cost is positive",
            r.catalyst_replacement.annualized_replacement_cost_USD > 0.0);
  checkTrue("including catalyst raises the unit cost",
            r.plant_economics_with_catalyst.unit_cost_USD_per_t >=
            r.base.plant_economics.unit_cost_USD_per_t);
  std::printf("       [INFO] $%.1f/t without catalyst, $%.1f/t with\n",
              r.base.plant_economics.unit_cost_USD_per_t,
              r.plant_economics_with_catalyst.unit_cost_USD_per_t);

  std::printf("\n[5] The placeholder inputs are declared, not disguised\n");
  checkTrue("the message names them as placeholders",
            r.message.find("placeholder") != std::string::npos);

  return report("hybrid plant aged");
}
