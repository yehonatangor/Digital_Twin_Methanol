// The recycle loop solved at a reduced catalyst activity

#include "flowsheet/co2_h2_plant_recycle_aged.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== aged recycle loop ===\n\n");

  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  const double h2  = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);
  (void)h2;   // the canonical config supplies hydrogen from its own ratio

  std::printf("[1] Activity 1.0 reproduces the fresh loop\n");
  flowsheet::AgedRecycleLoopConfig fresh_cfg;
  fresh_cfg.activity = 1.0;
  const auto aged1 = flowsheet::run_co2_h2_plant_with_recycle_aged(co2, 0.0, fresh_cfg);
  const auto fresh = flowsheet::run_co2_h2_plant_with_recycle(co2, 0.0, fresh_cfg.recycle_cfg);
  checkTrue("both ok", aged1.ok && fresh.ok);
  check("activity echoed back", aged1.activity, 1.0, 1e-12);
  checkRel("same overall conversion",
           aged1.recycle.co2_conversion_overall, fresh.co2_conversion_overall, 1e-12);
  checkRel("same methanol production",
           aged1.recycle.meoh_production_kg_s, fresh.meoh_production_kg_s, 1e-12);

  std::printf("\n[2] A decayed catalyst genuinely moves the loop\n");
  flowsheet::AgedRecycleLoopConfig old_cfg;
  old_cfg.activity = 0.6;
  const auto aged = flowsheet::run_co2_h2_plant_with_recycle_aged(co2, 0.0, old_cfg);
  checkTrue("ok", aged.ok);
  checkTrue("still converges at reduced activity", aged.recycle.converged);
  std::printf("       [INFO] a=1.0: %.4f kg/s, a=0.6: %.4f kg/s\n",
              aged1.recycle.meoh_production_kg_s, aged.recycle.meoh_production_kg_s);
  checkTrue("production does not rise as the catalyst decays",
            aged.recycle.meoh_production_kg_s <=
            aged1.recycle.meoh_production_kg_s * (1.0 + 1e-9));

  std::printf("\n[2b] The two design branches have different ageing envelopes\n");
  // Less activity means less conversion per pass, which means more recycle to
  // hold the same duty, which means a larger Ergun drop. The loop therefore has
  // an activity floor, and it is not the same on both branches. The canonical
  // point runs an 8x recycle and reaches that floor first; the compact branch
  // starts at 5x and survives deeper into the catalyst's life. This is the
  // third axis of the yield-against-hydraulics trade
  auto floor_reached = [&](double activity, double ratio, double recycle_fraction){
    flowsheet::AgedRecycleLoopConfig c;
    c.recycle_cfg.fresh_h2_to_co2_ratio = ratio;
    c.recycle_cfg.recycle_cfg.recycle_fraction = recycle_fraction;
    c.activity = activity;
    return !flowsheet::run_co2_h2_plant_with_recycle_aged(co2, 0.0, c).recycle.ok;
  };
  checkTrue("the canonical branch holds at activity 0.45",
            !floor_reached(0.45, 2.95, 0.99));
  checkTrue("and fails by activity 0.40",
            floor_reached(0.40, 2.95, 0.99));
  checkTrue("the compact branch is still running at activity 0.20",
            !floor_reached(0.20, 2.40, 0.95));

  std::printf("\n[3] Guards\n");
  flowsheet::AgedRecycleLoopConfig bad;
  bad.activity = 0.0;
  checkTrue("activity 0 is refused",
            !flowsheet::run_co2_h2_plant_with_recycle_aged(co2, 0.0, bad).ok);
  bad.activity = 1.5;
  checkTrue("activity above 1 is refused",
            !flowsheet::run_co2_h2_plant_with_recycle_aged(co2, 0.0, bad).ok);

  return report("aged recycle loop");
}
