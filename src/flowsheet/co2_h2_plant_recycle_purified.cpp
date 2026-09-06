#include "co2_h2_plant_recycle_purified.hpp"

#include "species.hpp"

namespace flowsheet {

PurifiedRecycleLoopResult run_co2_h2_plant_recycle_purified(
    double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
    const PurifiedRecycleLoopConfig& cfg) {
  PurifiedRecycleLoopResult res;

  res.recycle = run_co2_h2_plant_with_recycle(m_dot_CO2_fresh_kg_s, m_dot_H2_fresh_kg_s,
                                               cfg.recycle_cfg);
  if (!res.recycle.ok) {
    res.message = "recycle loop failed: " + res.recycle.message;
    return res;
  }

  // Let the crude down to an intermediate pressure and degas it before the
  // column. Real VLE, same machinery as the drum, so the split reflects actual
  // solubility rather than an assumption that all permanent gas leaves
  res.column_feed = res.recycle.knockout.liquid;
  if (cfg.use_letdown_flash && cfg.letdown_P_bar > 0.0) {
    res.letdown = front_end::separate(res.recycle.knockout.liquid, cfg.letdown_T_K,
                                       cfg.letdown_P_bar * 1.0e5);
    if (res.letdown.ok) {
      res.column_feed = res.letdown.liquid;
      for (const auto& kv : res.letdown.vapor.molar_flow) {
        if (kv.first == Species::CH3OH) {
          res.methanol_lost_to_letdown_mol_s += kv.second;
        } else if (kv.first != Species::H2O) {
          res.dissolved_gas_vented_mol_s += kv.second;
        }
      }
    }
  }

  // The column purifies the degassed condensate, not the crude figure
  res.distillation = front_end::purify_methanol(res.column_feed, cfg.distillation_cfg);
  if (!res.distillation.ok) {
    res.message = "distillation failed: " + res.distillation.message;
    return res;
  }

  const double M_meoh = properties(Species::CH3OH).molar_mass;
  const auto it = res.distillation.distillate.molar_flow.find(Species::CH3OH);
  const double meoh_mol_s = (it == res.distillation.distillate.molar_flow.end()) ? 0.0 : it->second;
  res.meoh_product_kg_s = res.distillation.distillate.totalMassFlow();
  if (!(res.meoh_product_kg_s > 0.0)) res.meoh_product_kg_s = meoh_mol_s * M_meoh / 1000.0;

  if (res.meoh_product_kg_s > 0.0) {
    res.crude_vs_distillate_mass_ratio =
        res.recycle.meoh_production_kg_s / res.meoh_product_kg_s;
  }

  res.ok = true;
  res.message =
      "ok. Production is the distillate's own mass flow, so separation recovery and purity are "
      "accounted for. The column's own capital cost is not in this result; size it with "
      "economics::size_distillation_column() and cost it separately.";
  return res;
}

}  // namespace flowsheet
