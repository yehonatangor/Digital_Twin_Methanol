// Price-threshold dispatch over a synthetic price series

#include "dispatch/dispatch_loop.hpp"
#include "_harness.inc"
#include <vector>

int main() {
  std::printf("=== price-threshold dispatch ===\n\n");

  dispatch::DispatchConfig cfg;
  cfg.electrolyzer_power_MW = 2.0;
  cfg.price_threshold_USD_per_MWh = 40.0;
  cfg.storage.V_m3 = 5000.0;
  cfg.meoh_demand_kg_h2_per_s = 0.0;
  cfg.dt_s = 3600.0;

  // 12 cheap hours then 12 expensive ones
  std::vector<double> prices;
  for (int h = 0; h < 12; ++h) prices.push_back(10.0);
  for (int h = 0; h < 12; ++h) prices.push_back(90.0);

  dispatch::StorageState s0; s0.M_H2_kg = 500.0;
  const auto r = dispatch::run_price_threshold_dispatch(prices, s0, cfg);

  std::printf("[1] The rule fires on price, and only on price\n");
  checkTrue("ok", r.ok);
  check("timesteps", static_cast<double>(r.steps.size()), 24.0, 0.0);
  check("steps on = the 12 cheap hours", static_cast<double>(r.n_steps_on), 12.0, 0.0);
  checkTrue("on during the cheap block",   r.steps[0].electrolyzer_on);
  checkTrue("off during the costly block", !r.steps[23].electrolyzer_on);

  std::printf("\n[2] Cost accrues only while running\n");
  checkTrue("a cheap step costs something", r.steps[0].electricity_cost_USD > 0.0);
  check("an idle step costs nothing", r.steps[23].electricity_cost_USD, 0.0, 1e-12);
  double sum = 0.0;
  for (const auto& s : r.steps) sum += s.electricity_cost_USD;
  check("total is the sum of the steps", r.total_electricity_cost_USD, sum, 1e-9);
  checkTrue("all electricity was bought at the cheap price",
            r.total_electricity_cost_USD > 0.0);

  std::printf("\n[3] Hydrogen accumulates with no draw\n");
  checkTrue("hydrogen was produced", r.total_h2_produced_kg > 0.0);
  check("nothing consumed with zero demand", r.total_h2_consumed_kg, 0.0, 1e-12);
  checkTrue("storage ended higher than it started", r.final_state.M_H2_kg > 500.0);

  std::printf("\n[4] Raising the threshold runs the electrolyzer more\n");
  auto hi = cfg; hi.price_threshold_USD_per_MWh = 100.0;
  const auto r2 = dispatch::run_price_threshold_dispatch(prices, s0, hi);
  check("all 24 hours now run", static_cast<double>(r2.n_steps_on), 24.0, 0.0);
  checkTrue("and it costs more", r2.total_electricity_cost_USD > r.total_electricity_cost_USD);

  std::printf("\n[5] A draw with no production trips the floor\n");
  auto draw = cfg;
  draw.price_threshold_USD_per_MWh = -1.0;   // never runs
  draw.meoh_demand_kg_h2_per_s = 1.0;
  dispatch::StorageState tiny; tiny.M_H2_kg = 100.0;
  const auto r3 = dispatch::run_price_threshold_dispatch(prices, tiny, draw);
  checkTrue("floor violations are counted", r3.n_floor_violations > 0);
  check("nothing ran", static_cast<double>(r3.n_steps_on), 0.0, 0.0);

  std::printf("\n[6] Guards\n");
  checkTrue("an empty price series is rejected",
            !dispatch::run_price_threshold_dispatch({}, s0, cfg).ok);
  auto bad = cfg; bad.dt_s = 0.0;
  checkTrue("non-positive dt is rejected",
            !dispatch::run_price_threshold_dispatch(prices, s0, bad).ok);

  return report("dispatch loop");
}
