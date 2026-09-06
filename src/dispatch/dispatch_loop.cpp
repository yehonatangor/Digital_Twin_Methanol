#include "dispatch_loop.hpp"

#include <algorithm>

namespace dispatch {

DispatchRunResult run_price_threshold_dispatch(
    const std::vector<double>& price_USD_per_MWh_series, const StorageState& initial_state,
    const DispatchConfig& cfg) {
  DispatchRunResult res;

  if (price_USD_per_MWh_series.empty()) {
    res.ok = false;
    res.message = "price series is empty";
    return res;
  }
  if (!(cfg.dt_s > 0.0)) {
    res.ok = false;
    res.message = "dt_s must be positive";
    return res;
  }

  StorageState state = initial_state;
  const double hours_per_step = cfg.dt_s / 3600.0;

  res.steps.reserve(price_USD_per_MWh_series.size());

  for (double price : price_USD_per_MWh_series) {
    DispatchTimestepResult step;
    step.price_USD_per_MWh = price;

    // Runs when power is at or below the threshold
    step.electrolyzer_on = (cfg.electrolyzer_power_MW > 0.0) &&
                           (price <= cfg.price_threshold_USD_per_MWh);

    double m_dot_prod_kg_s = 0.0;
    if (step.electrolyzer_on) {
      const front_end::ElectrolyzerInput in{cfg.electrolyzer_power_MW,
                                            cfg.electrolyzer_pressure_bar};
      step.electrolyzer = front_end::run_electrolyzer(in, cfg.electrolyzer_cfg);
      if (step.electrolyzer.ok) {
        m_dot_prod_kg_s = step.electrolyzer.m_dot_H2_kg_s;
        step.electricity_cost_USD =
            step.electrolyzer.P_total_MW * hours_per_step * price;
      }
    }

    step.storage_step = step_storage(state, m_dot_prod_kg_s, cfg.meoh_demand_kg_h2_per_s,
                                      cfg.dt_s, cfg.storage);
    state = step.storage_step.state;

    res.total_electricity_cost_USD += step.electricity_cost_USD;
    res.total_h2_produced_kg += m_dot_prod_kg_s * cfg.dt_s;
    res.total_h2_consumed_kg += step.storage_step.m_dot_consumed_actual_kg_s * cfg.dt_s;
    res.total_h2_vented_kg   += step.storage_step.m_dot_vented_kg_s * cfg.dt_s;
    if (step.storage_step.floor_violation)   ++res.n_floor_violations;
    if (step.storage_step.ceiling_violation) ++res.n_ceiling_violations;
    if (step.electrolyzer_on)                ++res.n_steps_on;

    res.steps.push_back(std::move(step));
  }

  res.final_state = state;
  res.ok = true;
  res.message = "ok";
  return res;
}

}
