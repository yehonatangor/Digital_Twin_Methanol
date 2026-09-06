// Aged recycle loop plus distillation

#include "flowsheet/co2_h2_plant_recycle_purified_aged.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== aged purified recycle loop ===\n\n");

  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  const double h2  = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);

  flowsheet::PurifiedRecycleLoopAgedConfig cfg;
  cfg.recycle_cfg.activity = 0.6;
  const auto r = flowsheet::run_co2_h2_plant_recycle_purified_aged(co2, h2, cfg);

  std::printf("[1] It runs end to end\n");
  checkTrue("ok", r.ok);
  checkTrue("the loop converged", r.recycle.recycle.converged);
  checkTrue("the column ran", r.distillation.ok);
  check("activity carried through", r.recycle.activity, 0.6, 1e-12);

  std::printf("\n[2] The distillate is the reported product\n");
  checkTrue("product is positive", r.meoh_product_kg_s > 0.0);
  checkTrue("distillate does not exceed the crude",
            r.meoh_product_kg_s <= r.recycle.recycle.meoh_production_kg_s + 1e-9);
  checkTrue("purity target met",
            r.distillation.actual_purity_wt_fraction >= 0.999 - 1e-6);

  std::printf("\n[3] Fresh catalyst produces at least as much\n");
  flowsheet::PurifiedRecycleLoopAgedConfig fresh;
  fresh.recycle_cfg.activity = 1.0;
  const auto rf = flowsheet::run_co2_h2_plant_recycle_purified_aged(co2, h2, fresh);
  checkTrue("ok", rf.ok);
  checkTrue("fresh yields at least as much product",
            rf.meoh_product_kg_s >= r.meoh_product_kg_s * (1.0 - 1e-9));

  return report("aged purified recycle loop");
}
