#include "hybrid_plant_recycle_purified_compressed.hpp"

#include <algorithm>

#include "species.hpp"

namespace flowsheet {

namespace {
constexpr double kSecondsPerYear = 365.0 * 24.0 * 3600.0;
}  // namespace

HybridPlantRecyclePurifiedCompressedResult run_hybrid_plant_recycle_purified_compressed(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecyclePurifiedCompressedConfig& cfg) {
  HybridPlantRecyclePurifiedCompressedResult res;

  if (!(cfg.co2_feed_kg_s > 0.0)) {
    res.message = "co2_feed_kg_s must be positive; it is the caller's own design point";
    return res;
  }

  // 1. Dispatch
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

  // 2. Reactor chain, at the steady-state design point
  const double h2_kg_s = presets::stoichiometric_h2_feed_kg_s(cfg.co2_feed_kg_s);
  res.reactor_chain =
      run_co2_h2_plant_recycle_purified(cfg.co2_feed_kg_s, h2_kg_s, cfg.purified_cfg);
  if (!res.reactor_chain.ok) {
    res.message = "reactor chain failed: " + res.reactor_chain.message;
    return res;
  }

  // 2b. The compression train is real, powered and costed here
  res.compression = run_compression_train(cfg.co2_feed_kg_s, h2_kg_s, cfg.compression_cfg);
  if (!res.compression.ok) {
    res.message = "compression train failed: " + res.compression.message;
    return res;
  }
  res.compression_power_MW = res.compression.total_power_MW;

  // 3. CAPEX
  std::vector<economics::CapexLineItem> items;
  items.push_back(economics::cost_reactor_as_shell_and_tube(
      "methanol synthesis reactor", cfg.purified_cfg.recycle_cfg.plant_cfg.reactor_cfg.bed.n_tubes > 1
          ? cfg.purified_cfg.recycle_cfg.plant_cfg.reactor_cfg.bed
          : presets::van_dal_catalyst_shi_tubes(), cfg.reactor_FM, cfg.reactor_FP));

  const double we_power_kW = cfg.dispatch_cfg.electrolyzer_power_MW * 1000.0;
  const economics::CapexLineItem we_item =
      economics::cost_electrolyzer("PEM electrolyzer", we_power_kW, cfg.electrolyzer_usd_per_kW);
  items.push_back(we_item);

  for (const auto& st : res.compression.co2_train.stages) {
    items.push_back(economics::cost_compressor("CO2 compressor stage", st.P_comp_MW * 1000.0,
                                                cfg.reactor_moc));
  }
  for (const auto& st : res.compression.h2_train.stages) {
    items.push_back(economics::cost_compressor("H2 compressor stage", st.P_comp_MW * 1000.0,
                                                cfg.reactor_moc));
  }

  res.capex = economics::aggregate_capex(items, cfg.cepci_target_year);
  if (!res.capex.ok) {
    res.message = "CAPEX aggregation failed: " + res.capex.message;
    return res;
  }

  // 4. OPEX. The CO2 basis is the FRESH feed, never the converged reactor inlet
  const double maintenance_base =
      res.capex.total_module_cost_escalated - res.capex.sum_all_in_installed_usd;
  const economics::WEReplacementResult we_repl = economics::we_replacement_cost(
      we_item.CBM_2001usd, cfg.we_stack_lifetime_years, cfg.project_years);

  economics::ReactantFlowsKgPerS flows;
  flows.co2_kg_s = cfg.co2_feed_kg_s;

  res.annual_opex = economics::compute_opex(
      flows, res.dispatch_avg_electricity_MW + res.compression_power_MW, cfg.reactant_prices,
      cfg.reactant_prices.stream_factor, maintenance_base, we_repl);
  if (!res.annual_opex.ok) {
    res.message = "OPEX failed: " + res.annual_opex.message;
    return res;
  }

  // Replace the flat-price electricity estimate with the dispatch run's own
  // price-weighted cost
  if (!price_USD_per_MWh_series.empty()) {
    const double run_s =
        static_cast<double>(price_USD_per_MWh_series.size()) * cfg.dispatch_cfg.dt_s;
    if (run_s > 0.0) {
      const double scale = kSecondsPerYear * cfg.reactant_prices.stream_factor / run_s;
      res.annual_opex.total_opex_USD_per_y -= res.annual_opex.electricity_cost_USD_per_y;
      res.annual_opex.electricity_cost_USD_per_y =
          res.dispatch.total_electricity_cost_USD * scale;
      // Compression runs whenever the plant runs, so it is charged at the
      // dispatch run's own average price rather than the threshold price
      const double avg_price =
          res.dispatch.total_electricity_cost_USD /
          std::max(1e-12, res.dispatch_avg_electricity_MW *
                              static_cast<double>(price_USD_per_MWh_series.size()) *
                              cfg.dispatch_cfg.dt_s / 3600.0);
      res.annual_opex.electricity_cost_USD_per_y +=
          res.compression_power_MW * (kSecondsPerYear * cfg.reactant_prices.stream_factor / 3600.0) *
          avg_price;
      res.annual_opex.total_opex_USD_per_y += res.annual_opex.electricity_cost_USD_per_y;
    }
  }

  // 5. Unit cost
  const double annual_production_t =
      res.reactor_chain.meoh_product_kg_s * kSecondsPerYear * cfg.reactant_prices.stream_factor / 1000.0;
  res.plant_economics = economics::run_plant_economics(
      res.capex.total_module_cost_escalated, res.annual_opex, cfg.reactant_prices.interest_rate,
      cfg.project_years, annual_production_t);

  res.ok = true;
  res.message = "ok. Compression is real, powered and costed. The distillation column's own capital "
      "cost is still not included. " + res.capex.message;
  return res;
}

}  // namespace flowsheet
