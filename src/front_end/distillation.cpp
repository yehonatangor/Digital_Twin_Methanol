#include "distillation.hpp"

namespace front_end {

namespace {

double mol_s_of(const Stream& s, Species sp) {
  auto it = s.molar_flow.find(sp);
  return (it == s.molar_flow.end()) ? 0.0 : it->second;
}

double kg_per_mol(Species sp) { return properties(sp).molar_mass / 1000.0; }

// Only methanol and water condense at column conditions. Everything else in
// this project's species set is a permanent gas and must leave as vent
bool is_condensable(Species sp) {
  return sp == Species::CH3OH || sp == Species::H2O;
}

}  // namespace

DistillationResult purify_methanol(const Stream& crude_methanol, const DistillationConfig& cfg) {
  DistillationResult r;

  if (!(cfg.methanol_recovery_fraction >= 0.0 && cfg.methanol_recovery_fraction <= 1.0)) {
    r.message = "methanol_recovery_fraction must be in [0, 1]";
    return r;
  }
  if (!(cfg.product_purity_wt_fraction > 0.0 && cfg.product_purity_wt_fraction <= 1.0)) {
    r.message = "product_purity_wt_fraction must be in (0, 1]";
    return r;
  }

  r.distillate.temperature = crude_methanol.temperature;
  r.distillate.pressure    = crude_methanol.pressure;
  r.distillate.phase       = Phase::Liquid;
  r.bottoms.temperature    = crude_methanol.temperature;
  r.bottoms.pressure       = crude_methanol.pressure;
  r.bottoms.phase          = Phase::Liquid;
  r.light_ends.temperature = crude_methanol.temperature;
  r.light_ends.pressure    = crude_methanol.pressure;
  r.light_ends.phase       = Phase::Vapor;

  const double meoh_in = mol_s_of(crude_methanol, Species::CH3OH);
  const double h2o_in  = mol_s_of(crude_methanol, Species::H2O);

  // 1. Recovery fraction of the feed methanol goes overhead
  const double meoh_dist = cfg.methanol_recovery_fraction * meoh_in;
  r.methanol_recovered_mol_s = meoh_dist;

  const double meoh_dist_kg = meoh_dist * kg_per_mol(Species::CH3OH);

  // 2. Add just enough water to land on the purity target, capped at what the
  //    feed actually carries:
  //      purity = m_meoh / (m_meoh + m_h2o)  =>  m_h2o = m_meoh (1-purity)/purity
  double h2o_dist_kg = 0.0;
  if (meoh_dist_kg > 0.0) {
    h2o_dist_kg = meoh_dist_kg * (1.0 - cfg.product_purity_wt_fraction) /
                  cfg.product_purity_wt_fraction;
  }
  const double h2o_in_kg = h2o_in * kg_per_mol(Species::H2O);
  const bool water_limited = h2o_dist_kg > h2o_in_kg;
  if (water_limited) h2o_dist_kg = h2o_in_kg;

  const double h2o_dist = (kg_per_mol(Species::H2O) > 0.0)
                              ? h2o_dist_kg / kg_per_mol(Species::H2O)
                              : 0.0;

  if (meoh_dist > 0.0) r.distillate.molar_flow[Species::CH3OH] = meoh_dist;
  if (h2o_dist  > 0.0) r.distillate.molar_flow[Species::H2O]   = h2o_dist;

  // 3. Condensables split between distillate and bottoms. Permanent gases
  //    cannot stay dissolved in the wastewater, so they leave as vent. A
  //    stabiliser or degassing drum upstream would do this in a real plant;
  //    here the column model accounts for them explicitly rather than burying
  //    them in a liquid stream
  for (const auto& [sp, n] : crude_methanol.molar_flow) {
    if (n <= 0.0) continue;
    if (!is_condensable(sp)) {
      r.light_ends.molar_flow[sp] = n;
      r.light_ends_mol_s += n;
      continue;
    }
    double to_bottoms = n;
    if (sp == Species::CH3OH) to_bottoms = n - meoh_dist;
    else if (sp == Species::H2O) to_bottoms = n - h2o_dist;
    if (to_bottoms > 0.0) r.bottoms.molar_flow[sp] = to_bottoms;
  }

  const double denom = meoh_dist_kg + h2o_dist_kg;
  r.actual_purity_wt_fraction = (denom > 0.0) ? meoh_dist_kg / denom : 0.0;

  r.ok = true;
  r.message = water_limited
                  ? "ok (water-limited: feed had less water than the purity target implies, "
                    "so the achieved purity exceeds the target)"
                  : "ok";
  return r;
}

}  // namespace front_end
