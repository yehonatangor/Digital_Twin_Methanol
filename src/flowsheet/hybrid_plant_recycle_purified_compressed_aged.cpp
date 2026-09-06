#include "hybrid_plant_recycle_purified_compressed_aged.hpp"

#include "reactor/ergun.hpp"

namespace flowsheet {

HybridPlantRecyclePurifiedCompressedAgedResult run_hybrid_plant_recycle_purified_compressed_aged(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecyclePurifiedCompressedAgedConfig& cfg) {
  HybridPlantRecyclePurifiedCompressedAgedResult res;

  // 1. The base run, unchanged
  res.base = run_hybrid_plant_recycle_purified_compressed(price_USD_per_MWh_series,
                                                           initial_storage_state, cfg.base_cfg);
  if (!res.base.ok) {
    res.message = "base plant failed: " + res.base.message;
    return res;
  }

  // 2. Activity trajectory at the reactor's own hot-end temperature
  const auto& loop = res.base.reactor_chain.recycle;
  res.aging_temperature_K = (cfg.aging_display_cfg.aging_T_K > 0.0)
                                ? cfg.aging_display_cfg.aging_T_K
                                : loop.reactor.outlet.T_K;

  res.trajectory_within_fichtl_fitted_range =
      cfg.aging_display_cfg.t_end_h <= kFichtlFittedRangeHours;

  res.activity_trajectory = activity_decay::integrate_activity_isothermal(
      1.0, cfg.aging_display_cfg.t_end_h, cfg.aging_display_cfg.n_steps, res.aging_temperature_K,
      cfg.aging_display_cfg.decay_cfg);

  // 3. Catalyst replacement as an OPEX line
  reactor::BedGeometry bed = cfg.base_cfg.purified_cfg.recycle_cfg.plant_cfg.reactor_cfg.bed;
  if (bed.n_tubes <= 1) bed = presets::van_dal_catalyst_shi_tubes();
  res.catalyst_mass_total_kg = reactor::catalyst_mass_total_kg(bed);

  res.catalyst_replacement = economics::catalyst_replacement_cost(
      res.catalyst_mass_total_kg, cfg.catalyst_replacement_cfg.price_USD_per_kg,
      cfg.catalyst_replacement_cfg.lifetime_years, cfg.base_cfg.project_years);

  // 4. Re-run the unit cost with the extra OPEX line, leaving the base alone
  economics::PlantOpexResult opex = res.base.annual_opex;
  opex.total_opex_USD_per_y += res.catalyst_replacement.annualized_replacement_cost_USD;

  res.plant_economics_with_catalyst = economics::run_plant_economics(
      res.base.capex.total_module_cost_escalated, opex, cfg.base_cfg.reactant_prices.interest_rate,
      cfg.base_cfg.project_years, res.base.plant_economics.annual_production_t);

  res.ok = true;
  res.message = res.trajectory_within_fichtl_fitted_range
                    ? "ok. Catalyst price and replacement interval are flagged placeholders, not "
                      "sourced from this project's library."
                    : "ok, but the trajectory runs past Fichtl's roughly 1600 h fitted range and "
                      "is extrapolated. Catalyst price and interval are flagged placeholders.";
  return res;
}

}  // namespace flowsheet
