#pragma once
// The recycle loop plus the distillation column
//
// Adds an intermediate let-down flash between the high-pressure knockout drum
// and the column, at 25 bar, through the same PR and NRTL machinery the drum
// uses. It strips most of the dissolved hydrogen and very little of the CO2,
// because methanol is a good physical solvent for CO2; that is the Rectisol
// mechanism and not a modelling failure
//
// Reported production is the DISTILLATE, not the crude
// See docs/17-separations.md

#include <string>
#include "flowsheet/co2_h2_plant_recycle.hpp"
#include "front_end/distillation.hpp"
#include "front_end/knockout_drum.hpp"

namespace flowsheet {

struct PurifiedRecycleLoopConfig {
  RecycleLoopConfig recycle_cfg;
  front_end::DistillationConfig distillation_cfg;  // defaults to Shi's own 99.5%/99.9wt%

  // Intermediate let-down flash between the high-pressure knockout drum and
  // the column. Sending 69 bar condensate straight into a 1.2 bar column skips
  // the degassing step a real plant performs, and the dissolved gas would
  // arrive at the column overhead instead
  //
  // Worth knowing what this does and does not achieve. It strips most of the
  // dissolved hydrogen, which is barely soluble. It removes very little of the
  // dissolved carbon dioxide, because methanol is a good physical solvent for
  // CO2, which is the basis of the Rectisol process. The flash is real VLE
  // through the same Peng-Robinson and NRTL machinery the drum uses, so it
  // reports what actually leaves rather than assuming all permanent gas does
  bool   use_letdown_flash = true;
  double letdown_P_bar = 25.0;    // mid-band of the usual 20 to 30 bar let-down
  double letdown_T_K   = 308.15;  // same 35 C, no intermediate reheat modelled
};

struct PurifiedRecycleLoopResult {
  RecycleLoopResult recycle;                 // full converged recycle-loop result, unchanged
  front_end::KnockoutDrumResult letdown;     // intermediate degassing flash, zeroed when disabled
  Stream column_feed;                        // what the column actually receives
  double dissolved_gas_vented_mol_s = 0.0;   // permanent gas removed by the let-down
  double methanol_lost_to_letdown_mol_s = 0.0;  // product carried off with that vent
  front_end::DistillationResult distillation; // purify_methanol() run on the degassed liquid

  double meoh_product_kg_s = 0.0;            // distillate's OWN mass flow -- the real product metric to use downstream
  double crude_vs_distillate_mass_ratio = 0.0; // recycle.meoh_production_kg_s / meoh_product_kg_s, expect ~1.00-1.01

  bool ok = false;
  std::string message;   // states the column-CAPEX gap explicitly, see header
};

PurifiedRecycleLoopResult run_co2_h2_plant_recycle_purified(
    double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
    const PurifiedRecycleLoopConfig& cfg = PurifiedRecycleLoopConfig{});

}  // namespace flowsheet
