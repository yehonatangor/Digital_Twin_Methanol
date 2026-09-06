#include "co2_h2_plant_recycle_purified_aged.hpp"

#include "species.hpp"

namespace flowsheet {

PurifiedRecycleLoopAgedResult run_co2_h2_plant_recycle_purified_aged(
    double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
    const PurifiedRecycleLoopAgedConfig& cfg) {
  PurifiedRecycleLoopAgedResult res;

  res.recycle = run_co2_h2_plant_with_recycle_aged(m_dot_CO2_fresh_kg_s, m_dot_H2_fresh_kg_s,
                                                    cfg.recycle_cfg);
  if (!res.recycle.ok) {
    res.message = "aged recycle loop failed: " + res.recycle.message;
    return res;
  }

  res.distillation =
      front_end::purify_methanol(res.recycle.recycle.knockout.liquid, cfg.distillation_cfg);
  if (!res.distillation.ok) {
    res.message = "distillation failed: " + res.distillation.message;
    return res;
  }

  res.meoh_product_kg_s = res.distillation.distillate.totalMassFlow();
  if (res.meoh_product_kg_s > 0.0) {
    res.crude_vs_distillate_mass_ratio =
        res.recycle.recycle.meoh_production_kg_s / res.meoh_product_kg_s;
  }

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace flowsheet
