#include "hybrid_plant.hpp"

#include "flowsheet/co2_h2_plant.hpp"
#include "species.hpp"

namespace flowsheet {

namespace {
constexpr double kSecondsPerYear = 365.0 * 24.0 * 3600.0;
}  // namespace

HybridPlantResult run_hybrid_plant(const std::vector<double>& price_USD_per_MWh_series,
                                    const dispatch::StorageState& initial_storage_state,
                                    const HybridPlantConfig& cfg) {
  HybridPlantResult res;

  if (!(cfg.co2_feed_kg_s > 0.0)) {
    res.message = "co2_feed_kg_s must be positive; it is the caller's own design point";
    return res;
  }

  // 1. Dispatch: the time-varying layer
  res.dispatch = dispatch::run_price_threshold_dispatch(price_USD_per_MWh_series,
                                                         initial_storage_state, cfg.dispatch_cfg);
  if (!res.dispatch.ok) {
    res.message = "dispatch failed: " + res.dispatch.message;
    return res;
  }

  double sum_MW = 0.0;
  for (const auto& s : res.dispatch.steps) sum_MW += s.electrolyzer.P_total_MW;
  if (!res.dispatch.steps.empty()) {
    res.dispatch_avg_electricity_MW = sum_MW / static_cast<double>(res.dispatch.steps.size());
  }

  // 2. Reactor chain at the steady-state design point
  const double h2_kg_s = presets::stoichiometric_h2_feed_kg_s(cfg.co2_feed_kg_s);
  res.reactor_chain = run_co2_h2_plant(cfg.co2_feed_kg_s, h2_kg_s, cfg.reactor_chain_cfg);
  if (!res.reactor_chain.ok) {
    res.message = "reactor chain failed: " + res.reactor_chain.message;
    return res;
  }

  // 3. CAPEX: reactor plus electrolyzer
  // The bed must be the one the reactor actually ran on. An unset config bed
  // resolves to the composite preset inside run_co2_h2_plant(), so costing the
  // config bed directly would silently produce a zero reactor line
  reactor::BedGeometry costed_bed = cfg.reactor_chain_cfg.reactor_cfg.bed;
  if (costed_bed.n_tubes <= 1) costed_bed = presets::van_dal_catalyst_shi_tubes();

  std::vector<economics::CapexLineItem> items;
  items.push_back(economics::cost_reactor_as_shell_and_tube(
      "methanol synthesis reactor", costed_bed, cfg.reactor_FM, cfg.reactor_FP));

  const double we_power_kW = cfg.dispatch_cfg.electrolyzer_power_MW * 1000.0;
  const economics::CapexLineItem we_item =
      economics::cost_electrolyzer("PEM electrolyzer", we_power_kW, cfg.electrolyzer_usd_per_kW);
  items.push_back(we_item);

  res.capex = economics::aggregate_capex(items, cfg.cepci_target_year);
  if (!res.capex.ok) {
    res.message = "CAPEX aggregation failed: " + res.capex.message;
    return res;
  }

  // 4. OPEX. Maintenance is charged against the non-electrolyzer CAPEX only
  const double maintenance_base = res.capex.total_module_cost_escalated
                                - res.capex.sum_all_in_installed_usd;
  const economics::WEReplacementResult we_repl = economics::we_replacement_cost(
      we_item.CBM_2001usd, cfg.we_stack_lifetime_years, cfg.project_years);

  economics::ReactantFlowsKgPerS flows;
  flows.co2_kg_s = cfg.co2_feed_kg_s;

  res.annual_opex = economics::compute_opex(flows, res.dispatch_avg_electricity_MW,
                                             cfg.reactant_prices, cfg.reactant_prices.stream_factor,
                                             maintenance_base, we_repl);
  if (!res.annual_opex.ok) {
    res.message = "OPEX failed: " + res.annual_opex.message;
    return res;
  }

  // The dispatch run already produced an exact, price-weighted electricity
  // cost, so use it in place of the flat-price estimate
  if (!price_USD_per_MWh_series.empty()) {
    const double run_s = static_cast<double>(price_USD_per_MWh_series.size())
                       * cfg.dispatch_cfg.dt_s;
    if (run_s > 0.0) {
      const double scale = kSecondsPerYear * cfg.reactant_prices.stream_factor / run_s;
      res.annual_opex.total_opex_USD_per_y -= res.annual_opex.electricity_cost_USD_per_y;
      res.annual_opex.electricity_cost_USD_per_y =
          res.dispatch.total_electricity_cost_USD * scale;
      res.annual_opex.total_opex_USD_per_y += res.annual_opex.electricity_cost_USD_per_y;
    }
  }

  // 5. Unit cost
  const double annual_production_t = res.reactor_chain.meoh_production_kg_s
                                   * kSecondsPerYear * cfg.reactant_prices.stream_factor / 1000.0;
  res.plant_economics = economics::run_plant_economics(
      res.capex.total_module_cost_escalated, res.annual_opex,
      cfg.reactant_prices.interest_rate, cfg.project_years, annual_production_t);

  res.ok = true;
  res.message = "ok. " + res.capex.message;
  return res;
}

}  // namespace flowsheet
