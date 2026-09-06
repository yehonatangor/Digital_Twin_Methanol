#pragma once
// CO2 + H2 to methanol with the recycle loop closed
//
// Composes co2_h2_plant.hpp rather than editing it. Tear stream is the
// recycle, solved by Wegstein-accelerated successive substitution: mix fresh
// feeds with the recycle guess, pre-heat, react per tube, scale to plant,
// cool, flash, split recycle from purge, compare, repeat
//
// The drum runs at the reactor's own outlet pressure and a circulator restores
// the loop pressure, so the Ergun drop is paid for rather than discarded
//
// The loop's feasibility boundary is bed hydraulics: mass flux grows until the
// Ergun drop consumes the loop pressure inside one RK4 sub-step. It moves with
// the thermal mode and the feed ratio, and the feasible set is NOT convex (at
// 1 % purge the loop closes at R = 2.70 and 2.85 but not at 2.75 or 2.80)
// Measured grid: tools/operating_envelope.cpp, docs/figures/03-*.svg
//
// Economics must charge m_dot_CO2_fresh_kg_s, not the reactor inlet flow,
// which is larger by the recycle ratio
//
// See docs/23-flowsheet-composition.md and docs/17-separations.md

#include <string>
#include "stream.hpp"
#include "reactor/reactor_core.hpp"
#include "front_end/knockout_drum.hpp"
#include "front_end/recycle.hpp"
#include "front_end/membrane.hpp"
#include "front_end/compressor_train.hpp"
#include "front_end/heat_exchanger.hpp"
#include "flowsheet/co2_h2_plant.hpp"

namespace flowsheet {

// Midpoint of the 5 to 25 bar band typical of polymeric H2 membrane permeate
// Engineering guidance, not sourced from any paper in this library
inline constexpr double kTypicalPermeatePressureBar = 15.0;
inline constexpr bool   kPermeatePressureSourced = false;

// Fresh hydrogen policy. The reverse shift takes one H2 per CO2, so the loop
// consumes 3 - 2*(CO selectivity), measured 2.695, not the stoichiometric 3.0
// Feeding above that circulates a surplus; it sets the recycle ratio, not the
// feasibility. See docs/17-separations.md
enum class FeedRatioMode {
  FromArgument,   // use m_dot_H2_fresh_kg_s as passed; the ratio field is ignored
  FixedRatio,     // H2 is computed from CO2 and fresh_h2_to_co2_ratio
  TargetInletSN   // outer secant on the ratio to hit target_inlet_SN
};

struct FeedRatioResult {
  FeedRatioMode mode = FeedRatioMode::FromArgument;
  double ratio_used = 0.0;          // molar H2 per mol CO2 actually fed
  double inlet_SN_achieved = 0.0;
  int    outer_iterations = 0;
  bool   converged = true;          // false if the target could not be reached
  bool   target_reachable = true;   // false if the search hit a bound
  std::string message;
};

struct RecycleLoopConfig {
  Co2H2PlantConfig plant_cfg;                 // reactor inlet T/P, knockout T/P, bed geometry -- reused as-is
  // 1 % purge. SOURCED: Van-Dal Sec. 2.3.2, "Some of the non-reacted gases
  // (1%) are purged". The model does not require it: CO leaves dissolved in
  // the crude liquid, so the loop converges at 0 % purge too
  front_end::RecycleConfig recycle_cfg{0.99};

  // Wegstein on the tear stream, per component, bounded:
  //   q = (g_n - g_{n-1})/(x_n - x_{n-1}),  x_{n+1} = x_n + (g_n - x_n)/(1 - q)
  // Load-bearing, not a convenience: plain substitution does not converge at
  // the design point within max_iterations
  bool   use_wegstein = true;
  double wegstein_q_min = -5.0;   // acceleration bound
  double wegstein_q_max = 0.95;   // a monotone loop gives positive q; bounding at 0 disables it

  int    max_iterations = 600;   // 115 passes at the design point with Wegstein
  double tol_rel = 1e-6;    // convergence: max relative change in any recycle-stream species' molar flow
  // Damping, (0,1]. Off by default because no one value is right everywhere:
  // 0.7 takes the hardest node from 291 passes to 165 but takes the compact
  // branch from 111 to 306. Exposed rather than chosen
  double relaxation = 1.0;

  bool knockout_at_reactor_outlet_pressure = true;   // false restores a fixed drum pressure

  // Fresh hydrogen policy, see FeedRatioMode above
  FeedRatioMode feed_ratio_mode = FeedRatioMode::FixedRatio;
  // 2.95. NOT sourced; chosen at the knee of the yield-against-compression
  // curve. Feeding 3.00 instead costs 14 MW for 0.85 points of carbon yield
  // R below 3.0 caps carbon yield at R/3, here 98.3 %
  double fresh_h2_to_co2_ratio = 2.95;
  double target_inlet_SN = 2.10;        // used by TargetInletSN
  double feed_ratio_min = 1.50;         // outer search bounds
  double feed_ratio_max = 4.00;
  double feed_ratio_tol = 1.0e-3;       // on the SN error
  int    feed_ratio_max_iter = 12;

  // Feed-effluent exchanger, Van-Dal Sec. 2.3.1 HX4. The reactor inlet
  // temperature is specified, so this does NOT move the converged fixed point;
  // it only splits heating between recovered and external duty
  bool   use_fehe = true;
  double fehe_UA_W_per_K = 5.0e6;   // effectiveness 0.90 at the design point
  front_end::FlowArrangement fehe_arrangement = front_end::FlowArrangement::Counterflow;
  front_end::CompressorConfig recycle_compressor_cfg;

  // Purge membrane. The retentate leaves as purge; the permeate is either
  // vented with it or routed back to the feed
  front_end::MembraneConfig membrane_cfg;
  bool   route_permeate_to_feed = true;
  // Negative: permeate leaves at the membrane's own feed pressure, the
  // module's stated contract and the optimistic case. A real value models a
  // membrane that drops it and pays to recompress. Case used is reported back
  double membrane_permeate_P_bar = -1.0;
};

struct RecycleLoopResult {
  Stream fresh_feed;                        // CO2 + H2 fresh feed ONLY (mol/s), never includes recycle
  Stream converged_reactor_inlet;            // fresh + recycle, at the converged fixed point
  reactor::ReactorResult reactor;            // PER-TUBE result of the LAST (converged) pass
  Stream reactor_outlet_plant_scale;         // last pass, plant-scale
  front_end::KnockoutDrumResult knockout;    // last pass
  front_end::RecycleSplitResult recycle_split;  // converged recycle/purge split

  double co2_conversion_per_pass = 0.0;      // last pass only -- same definition as co2_h2_plant.hpp
  double co2_conversion_overall = 0.0;       // (fresh CO2 in - CO2 leaving via purge+liquid) / fresh CO2 in -- the Van-Dal Table 5-comparable figure
  double meoh_production_kg_s = 0.0;         // methanol LEAVING the loop boundary (liquid + trace in purge), steady-state net production
  int    n_tubes_used = 0;

  int    n_iterations = 0;
  double final_residual = 0.0;               // the actual max relative change at the last iteration
  bool   converged = false;

  bool   ok = false;
  std::string message;

  // Pressure loop and purge recovery
  front_end::CompressorResult circulator;      // recycle stream back to reactor inlet pressure
  front_end::MembraneResult   purge_membrane;
  front_end::CompressorResult permeate_booster;
  // True when membrane_permeate_P_bar was left negative, so the permeate was
  // taken at the membrane's feed pressure. That is the optimistic case and
  // understates the real recovery cost; a caller reporting a compression bill
  // should say which case produced it
  bool   permeate_pressure_assumed_at_feed = true;
  double permeate_delivery_P_bar = 0.0;         // pressure the booster actually lifted from
  // Heat integration. feed_preheat_duty_MW is the total heat needed to bring
  // the mixed feed from its delivered temperature to the reactor inlet. The
  // exchanger supplies fehe.duty_MW of it and the trim heater the remainder
  front_end::TwoStreamHXResult fehe;      // zeroed when use_fehe is false
  double feed_preheat_duty_MW = 0.0;
  double trim_heater_duty_MW  = 0.0;      // external heat still required
  double trim_cooler_duty_MW  = 0.0;      // external cooling still required, negative

  // Loop diagnostics. A loop can be converged and mass-balanced while sitting
  // at an operating point no one would choose; these are what reveal it
  // loop_stoichiometric_number is Lim Eq. (4) at the CONVERGED inlet, not the
  // fresh feed. peak_bed_temperature_K is the profile maximum, not the outlet
  FeedRatioResult feed_ratio;       // what the hydrogen policy actually did
  double loop_stoichiometric_number = 0.0;
  double peak_bed_temperature_K = 0.0;
  double tail_gas_LHV_MW = 0.0;
  double product_LHV_MW = 0.0;

  double effluent_cooling_duty_MW = 0.0;        // reactor outlet down to the drum temperature
  double effluent_latent_fraction = 0.0;
};

RecycleLoopResult run_co2_h2_plant_with_recycle(double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
                                                 const RecycleLoopConfig& cfg = RecycleLoopConfig{});

}  // namespace flowsheet
