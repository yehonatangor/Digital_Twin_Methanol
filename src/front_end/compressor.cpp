#include "compressor.hpp"

#include <cmath>
#include "thermo.hpp"
#include "units.hpp"

namespace front_end {

CompressorResult run_compressor(const CompressorInput& in, const CompressorConfig& cfg) {
  CompressorResult r;

  if (!(in.m_dot_H2_kg_s >= 0.0)) {
    r.message = "m_dot_H2_kg_s must be non-negative";
    return r;
  }
  if (!(in.P_in_bar > 0.0) || !(in.P_out_bar > 0.0)) {
    r.message = "pressures must be positive";
    return r;
  }
  if (in.P_out_bar < in.P_in_bar) {
    r.message = "P_out_bar < P_in_bar -- not a compression (beta < 1)";
    return r;
  }
  if (!(cfg.T_in_K > 0.0)) {
    r.message = "T_in_K must be positive";
    return r;
  }
  if (!(cfg.eta_isentropic > 0.0 && cfg.eta_isentropic <= 1.0) ||
      !(cfg.eta_mechanical > 0.0 && cfg.eta_mechanical <= 1.0)) {
    r.message = "eta_isentropic and eta_mechanical must be in (0, 1]";
    return r;
  }

  r.beta = in.P_out_bar / in.P_in_bar;

  // cp on a MASS basis, from the project's own heat capacity correlation
  // rather than a second, independently sourced hydrogen constant
  const double cp_molar = thermo::cp(Species::H2, cfg.T_in_K);
  const double M_kg_per_mol = properties(Species::H2).molar_mass / 1000.0;
  r.cp_J_per_kgK = cp_molar / M_kg_per_mol;

  // Ideal-gas Mayer relation on a molar basis; k is basis-independent
  const double cv_molar = cp_molar - units::R;
  if (!(cv_molar > 0.0)) {
    r.message = "degenerate Cv (Cp <= R) -- cannot form heat-capacity ratio k";
    return r;
  }
  r.k_isentropic_exponent = cp_molar / cv_molar;

  // Mucci Sec. 3.3:
  //   P = m_dot cp T_in / (eta_is eta_mec) * (beta^((k-1)/k) - 1)
  const double exponent  = (r.k_isentropic_exponent - 1.0) / r.k_isentropic_exponent;
  const double work_term = std::pow(r.beta, exponent) - 1.0;
  const double P_comp_W  = in.m_dot_H2_kg_s * r.cp_J_per_kgK * cfg.T_in_K /
                           (cfg.eta_isentropic * cfg.eta_mechanical) * work_term;

  r.P_comp_MW = P_comp_W / 1.0e6;
  r.P_cooling_MW = r.P_comp_MW;   // Mucci states cooling demand equals compression power

  if (cfg.capacity_per_unit_kg_s > 0.0 && cfg.n_units_parallel >= 1) {
    const double per_unit_flow = in.m_dot_H2_kg_s / cfg.n_units_parallel;
    const double min_flow = cfg.min_load_fraction_single * cfg.capacity_per_unit_kg_s;
    r.below_min_turndown = per_unit_flow < min_flow;
  }

  r.ok = true;
  r.message = r.below_min_turndown
                  ? "ok (WARNING: below advisory minimum turndown -- see compressor.hpp header)"
                  : "ok";
  return r;
}

namespace presets {

CompressorConfig mucci_h2_compressor() { return CompressorConfig{}; }

CompressorInput mucci_table_a2_case(double beta) {
  CompressorInput in;
  in.m_dot_H2_kg_s = 1900.0 / 3600.0;   // 1900 kg/h
  in.P_in_bar  = 40.0;
  in.P_out_bar = 40.0 * beta;
  return in;
}

}  // namespace presets

}  // namespace front_end
