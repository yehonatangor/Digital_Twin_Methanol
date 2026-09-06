#pragma once
// PEM electrolyser with load-dependent efficiency
//
// Mucci et al. (2023) Table 2 / App. A.1, efficiency as a polynomial in power
// and pressure:
//   eta = a00 + a10*P + a20*P^2 + a01*p        (P in MW, p in bar)
//   a00 0.813, a10 -1.010e-1, a20 +1.397e-2, a01 -3.118e-4
// Fitted over 20-40 bar. Efficiency FALLS with load, so part-load operation is
// more efficient per unit hydrogen, which is what makes dispatch non-trivial
//
// Hydrogen LHV is derived from the formation-enthalpy table rather than
// tabulated, so one number governs it and thermo:: together
// Cooling duty is the shortfall from unity efficiency
// See docs/15-electrolysis.md

#include <string>
#include "species.hpp"

namespace front_end {

// Table A.1 polynomial coefficients for eta_PEM(P_mod, p_PEM), LHV basis
struct PemEfficiencyPoly {
  double a00;         // [-]
  double a10;         // [MW^-1]
  double a20;         // [MW^-2]
  double a01;         // [bar^-1]
};

struct ElectrolyzerConfig {
  int n_modules = 1;                       // number of parallel PEM modules

  double module_nominal_power_MW = 2.0;    // Table 2, sourced default
  double module_max_power_MW     = 2.5;    // Table 2, sourced default (125% of nominal)

  // Table A.1 fit validity window. Outside this range the polynomial is an
  // extrapolation, not a validated fit -- run_electrolyzer() still evaluates
  // it (so the caller gets a number, not a hard failure) but flags
  // ElectrolyzerResult::in_fit_range = false
  double P_mod_fit_min_MW = 0.2;
  double P_mod_fit_max_MW = 2.5;
  double p_PEM_fit_min_bar = 20.0;
  double p_PEM_fit_max_bar = 40.0;

  double faradaic_efficiency = 1.0;        // Mucci Sec. 3.2 assumption (i)
  double auxiliary_power_fraction = 0.05;  // Mucci Sec. 3.2 text, flat 5%

  PemEfficiencyPoly eff{0.813, -1.010e-1, 1.397e-2, -3.118e-4};  // Table A.1
};

struct ElectrolyzerInput {
  double P_PEM_MW;   // total DC power delivered to the electrolysis island (all modules)
  double p_PEM_bar;  // operating pressure
};

struct ElectrolyzerResult {
  double P_mod_MW = 0.0;         // power per module = P_PEM / n_modules
  double eta_PEM_LHV = 0.0;      // module efficiency, LHV basis

  double m_dot_H2_kg_s = 0.0;    // hydrogen production rate
  double m_dot_H2O_kg_s = 0.0;   // stoichiometric water consumption
  double m_dot_O2_kg_s = 0.0;    // stoichiometric oxygen byproduct

  double P_cooling_MW = 0.0;     // waste heat = P_PEM * (1 - eta_PEM), energy-balance remainder
  double P_auxiliary_MW = 0.0;   // 5% of P_PEM (power electronics, pumps)
  double P_total_MW = 0.0;       // P_PEM + P_auxiliary: total grid/DC draw of the island

  bool in_fit_range = true;      // false if (P_mod, p_PEM) fell outside Table A.1's fitted window
  bool ok = false;
  std::string message;
};

// LHV of hydrogen [MJ/kg], derived from thermo::deltaH() -- see header note
// above. Exposed as a function (not a stored constant) so it always tracks
// species.hpp's formation-enthalpy table
double pem_lhv_h2_MJ_per_kg();

// Evaluates Table A.1's polynomial directly; no side effects, no clamping
double pem_efficiency(double P_mod_MW, double p_PEM_bar, const PemEfficiencyPoly& eff);

ElectrolyzerResult run_electrolyzer(const ElectrolyzerInput& in, const ElectrolyzerConfig& cfg);

// -----------------------------------------------------------------------------
// Presets -- literature convenience case only. The engine itself never
// hardcodes these; ElectrolyzerConfig's own defaults already ARE Mucci's
// Table 2 / Table A.1 values, so this preset exists mainly for symmetry with
// the rest of the project's presets:: namespaces and as an explicit,
// named "this is the sourced literature case" entry point
// -----------------------------------------------------------------------------
namespace presets {

// Mucci Table 2 + Table A.1, single module, defaults unchanged
ElectrolyzerConfig mucci_pem_module();
inline constexpr bool kMucciPemModuleSourced = true;  // no placeholders in this module

}  // namespace presets

}  // namespace front_end
