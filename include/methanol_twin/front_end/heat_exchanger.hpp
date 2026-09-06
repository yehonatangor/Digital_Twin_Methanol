#pragma once
// Single-stream heating and cooling, and a two-stream recuperative exchanger
//
// Heat to a target temperature: direct enthalpy difference. Heat by a stated
// duty: Newton on the same expression, using stream cp as the exact
// derivative since dH/dT = cp from the same correlation
//
// Two-stream by effectiveness-NTU (the rating case, area known, outlets not):
//   eps = Q/Qmax,  NTU = UA/Cmin,  Cr = Cmin/Cmax
//   Qmax = Cmin*(Th_in - Tc_in)
// Counterflow and parallel forms. A counterflow temperature crossover is
// legitimate and is not guarded against
//
// UA HAS NO DEFAULT. No source paper in this library publishes one, and
// inventing a U would create exactly the unsourced free parameter this
// project exists to avoid. The caller supplies it
// See docs/16-compression-and-heat-exchange.md

#include <string>
#include "stream.hpp"

namespace front_end {

struct HXConfig {
  int    max_iter   = 100;
  double T_tol_K     = 1e-6;
  double T_floor_K   = 100.0;   // Newton guard rails, not physical bounds
  double T_ceiling_K = 3000.0;
};

struct HXResult {
  Stream outlet;
  double duty_MW = 0.0;   // + = heat added to the process stream, - = heat removed
  bool   ok = false;
  std::string message;
};

// Heats or cools to T_target_K at constant composition and pressure
HXResult heat_to_temperature(const Stream& in, double T_target_K);

// Inverse: given a duty [MW], solves for outlet T by Newton on
// thermo::enthalpy(), with stream heat capacity as the exact derivative
HXResult heat_with_duty(const Stream& in, double duty_MW, const HXConfig& cfg = HXConfig{});

// -----------------------------------------------------------------------------
// Condensing cooler
//
// WHY THIS IS SEPARATE FROM heat_to_temperature(). Both functions above are
// SINGLE-PHASE, IDEAL-GAS energy balances: Q = sum_i F_i*(H_i(T2) - H_i(T1))
// using thermo::enthalpy(), which knows nothing about phase change. That is
// exactly right for a gas-phase preheat, and exactly wrong for the cooling
// duties this flowsheet actually spends most of its energy on -- taking a
// reactor effluent full of methanol and water from ~500 K down to 313 K ahead
// of a knockout drum. A pure gas-phase method would return a "vapor" outlet 
// below its own dew point and a duty understated by the entire latent heat
//
// This function does the same job honestly, by reusing machinery the project
// already has and already tests rather than adding new physics:
//
//   1. flash::solve() at (T_target, P) determines the real phase split
//   2. The duty is the enthalpy difference between the single-phase inlet and
//      the two-phase outlet, INCLUDING the enthalpy of condensation, which is
//      obtained from thermo::enthalpy() plus the latent term implied by each
//      condensing species' Clausius-Clapeyron slope through
//      vaporPressure() -- i.e. from species.hpp's own DIPPR-101 fit, not a
//      separate hard-coded heat-of-vaporisation table that could drift out of
//      step with it
//
// Returns BOTH outlet phases so the caller can hand the liquid straight to a
// knockout drum and the vapour onward, without re-flashing
// -----------------------------------------------------------------------------
struct CondensingCoolResult {
  Stream vapor_outlet;
  Stream liquid_outlet;
  double duty_MW = 0.0;              // negative = heat removed
  double latent_fraction = 0.0;      // share of |duty| from condensation
  double vapor_fraction = 0.0;       // molar
  bool   condensed = false;          // false reduces to heat_to_temperature()
  bool   flash_converged = false;
  int    ideal_binary_pairs = 0;     // from flash.hpp
  bool   ok = false;
  std::string message;
};

// P_Pa <= 0 uses the inlet stream's own pressure
CondensingCoolResult cool_with_condensation(const Stream& in, double T_target_K,
                                             double P_Pa = -1.0);

// Two-stream recuperative exchanger. UA_W_per_K has no default
enum class FlowArrangement { Counterflow, Parallelflow };

struct TwoStreamHXResult {
  Stream hot_outlet;
  Stream cold_outlet;
  double duty_MW = 0.0;
  double effectiveness = 0.0;
  double NTU = 0.0;
  double C_min_W_per_K = 0.0;
  double C_max_W_per_K = 0.0;
  bool   ok = false;
  std::string message;
};

TwoStreamHXResult exchange_two_streams(const Stream& hot_in, const Stream& cold_in,
                                        double UA_W_per_K,
                                        FlowArrangement arrangement = FlowArrangement::Counterflow,
                                        const HXConfig& cfg = HXConfig{});

namespace presets {

// Van-Dal Table A.3 reactor inlet temperature, 220 C
// Van-Dal quotes two reactor temperatures. Sec. 2.3.1 heats the INDUSTRIAL
// feed to 210 C; Table A.3's single-tube LABORATORY case runs at 220 C
// Naming them apart stops one being used where the other belongs
double van_dal_industrial_reactor_inlet_T_K();   // 483.15 K, Sec. 2.3.1 (HX4)
double van_dal_lab_reactor_T_K();                // 493.15 K, Table A.3

}  // namespace presets

}  // namespace front_end
