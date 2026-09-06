#include "hybrid_plant_design_point.hpp"

#include <algorithm>

#include "flowsheet/co2_h2_plant.hpp"
#include "flowsheet/co2_h2_plant_recycle_purified_aged.hpp"
#include "species.hpp"

namespace flowsheet {

namespace {
constexpr double kSecondsPerYear = 365.0 * 24.0 * 3600.0;
}  // namespace

HybridPlantDesignPointResult evaluate_design_point(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantDesignPointConfig& cfg) {
  HybridPlantDesignPointResult res;

  if (!(cfg.co2_feed_kg_s > 0.0)) {
    res.message = "co2_feed_kg_s must be positive";
    return res;
  }
  if (!(cfg.activity > 0.0) || cfg.activity > 1.0) {
    res.message = "activity must be in (0, 1]";
    return res;
  }

  // Resolve the two swept variables
  res.bed_used = cfg.bed;
  if (res.bed_used.n_tubes <= 1) res.bed_used = presets::van_dal_catalyst_shi_tubes();
  res.activity_used = cfg.activity;

  // 1. Dispatch
  res.dispatch = dispatch::run_price_threshold_dispatch(price_USD_per_MWh_series,
                                                         initial_storage_state, cfg.dispatch_cfg);
  if (!res.dispatch.ok) {
    res.message = "dispatch failed: " + res.dispatch.message;
    return res;
  }
  double sum_MW = 0.0;
  for (const auto& s : res.dispatch.steps) sum_MW += s.electrolyzer.P_total_MW;
  const double dispatch_avg_MW =
      res.dispatch.steps.empty() ? 0.0 : sum_MW / static_cast<double>(res.dispatch.steps.size());

  // 2. Reactor chain at this geometry and activity
  PurifiedRecycleLoopAgedConfig chain_cfg;
  chain_cfg.recycle_cfg.activity = cfg.activity;
  chain_cfg.recycle_cfg.recycle_cfg.plant_cfg.reactor_cfg.bed = res.bed_used;
  chain_cfg.recycle_cfg.recycle_cfg.plant_cfg.reactor_inlet_T_K = cfg.reactor_inlet_T_K;
  chain_cfg.recycle_cfg.recycle_cfg.plant_cfg.reactor_inlet_P_bar = cfg.reactor_inlet_P_bar;
  chain_cfg.recycle_cfg.recycle_cfg.recycle_cfg.recycle_fraction = cfg.recycle_fraction;
  chain_cfg.distillation_cfg = cfg.distillation_cfg;

  const double h2_kg_s = presets::stoichiometric_h2_feed_kg_s(cfg.co2_feed_kg_s);
  const PurifiedRecycleLoopAgedResult aged =
      run_co2_h2_plant_recycle_purified_aged(cfg.co2_feed_kg_s, h2_kg_s, chain_cfg);

  // Present the aged run through the same shape the fresh purified loop uses,
  // so downstream consumers do not need to know which path produced it
  res.reactor_chain.recycle      = aged.recycle.recycle;
  res.reactor_chain.distillation = aged.distillation;
  res.reactor_chain.meoh_product_kg_s = aged.meoh_product_kg_s;
  res.reactor_chain.crude_vs_distillate_mass_ratio = aged.crude_vs_distillate_mass_ratio;
  res.reactor_chain.ok = aged.ok;
  res.reactor_chain.message = aged.message;

  if (!aged.ok) {
    res.message = "reactor chain failed: " + aged.message;
    return res;
  }
  res.co2_conversion_overall = aged.recycle.recycle.co2_conversion_overall;
  res.meoh_product_kg_s = aged.meoh_product_kg_s;

  // 3. Compression
  res.compression = run_compression_train(cfg.co2_feed_kg_s, h2_kg_s, cfg.compression_cfg);
  if (!res.compression.ok) {
    res.message = "compression train failed: " + res.compression.message;
    return res;
  }
  res.total_electricity_MW = dispatch_avg_MW + res.compression.total_power_MW;

  // 4. CAPEX
  std::vector<economics::CapexLineItem> items;
  items.push_back(economics::cost_reactor_as_shell_and_tube("methanol synthesis reactor",
                                                             res.bed_used, cfg.reactor_FM,
                                                             cfg.reactor_FP));
  const double we_power_kW = cfg.dispatch_cfg.electrolyzer_power_MW * 1000.0;
  const economics::CapexLineItem we_item =
      economics::cost_electrolyzer("PEM electrolyzer", we_power_kW, cfg.electrolyzer_usd_per_kW);
  items.push_back(we_item);
  for (const auto& st : res.compression.co2_train.stages) {
    items.push_back(
        economics::cost_compressor("CO2 compressor stage", st.P_comp_MW * 1000.0, cfg.reactor_moc));
  }
  for (const auto& st : res.compression.h2_train.stages) {
    items.push_back(
        economics::cost_compressor("H2 compressor stage", st.P_comp_MW * 1000.0, cfg.reactor_moc));
  }

  res.capex = economics::aggregate_capex(items, cfg.cepci_target_year);
  if (!res.capex.ok) {
    res.message = "CAPEX aggregation failed: " + res.capex.message;
    return res;
  }

  // 5. OPEX
  const double maintenance_base =
      res.capex.total_module_cost_escalated - res.capex.sum_all_in_installed_usd;
  const economics::WEReplacementResult we_repl = economics::we_replacement_cost(
      we_item.CBM_2001usd, cfg.we_stack_lifetime_years, cfg.project_years);

  economics::ReactantFlowsKgPerS flows;
  flows.co2_kg_s = cfg.co2_feed_kg_s;

  res.annual_opex =
      economics::compute_opex(flows, res.total_electricity_MW, cfg.reactant_prices,
                               cfg.reactant_prices.stream_factor, maintenance_base, we_repl);
  if (!res.annual_opex.ok) {
    res.message = "OPEX failed: " + res.annual_opex.message;
    return res;
  }

  // 6. Unit cost
  res.annual_production_t =
      res.meoh_product_kg_s * kSecondsPerYear * cfg.reactant_prices.stream_factor / 1000.0;
  res.plant_economics = economics::run_plant_economics(
      res.capex.total_module_cost_escalated, res.annual_opex, cfg.reactant_prices.interest_rate,
      cfg.project_years, res.annual_production_t);
  res.unit_cost_USD_per_t = res.plant_economics.unit_cost_USD_per_t;

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace flowsheet
