#pragma once
// =============================================================================
// co2_h2_plant_recycle_purified_aged.hpp -- aged recycle loop + real
// distillation, so an activity sweep also reports a real product stream
// =============================================================================
// WHY THIS IS A SEPARATE FILE, NOT AN EDIT. Same pattern as every prior
// layer: co2_h2_plant_recycle_purified.hpp (Chapter 8.6) composes the
// FRESH-catalyst recycle loop with front_end::purify_methanol(); this file
// is the identical composition, one call swapped -- run_co2_h2_plant_with_
// recycle_aged() (this chapter) in place of run_co2_h2_plant_with_recycle()
// -- so a caller sweeping activity gets a real, recovery/purity-accounted
// product stream at every point, not just crude methanol
// =============================================================================

#include <string>
#include "flowsheet/co2_h2_plant_recycle_aged.hpp"
#include "front_end/distillation.hpp"

namespace flowsheet {

struct PurifiedRecycleLoopAgedConfig {
  AgedRecycleLoopConfig recycle_cfg;
  front_end::DistillationConfig distillation_cfg;
};

struct PurifiedRecycleLoopAgedResult {
  AgedRecycleLoopResult recycle;
  front_end::DistillationResult distillation;

  double meoh_product_kg_s = 0.0;
  double crude_vs_distillate_mass_ratio = 0.0;

  bool ok = false;
  std::string message;
};

PurifiedRecycleLoopAgedResult run_co2_h2_plant_recycle_purified_aged(
    double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
    const PurifiedRecycleLoopAgedConfig& cfg = PurifiedRecycleLoopAgedConfig{});

}  // namespace flowsheet
