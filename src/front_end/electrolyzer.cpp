#include "electrolyzer.hpp"

#include "thermo.hpp"

namespace front_end {

double pem_lhv_h2_MJ_per_kg() {
  // Lower heating value is combustion to GASEOUS water. Writing the reaction
  // that way is what makes it the lower value rather than the higher, and the
  // formation data comes from the same table the reactor energy balance uses
  //   H2 + 1/2 O2 -> H2O(g)
  const thermo::Reaction combustion = {
      {Species::H2, -1.0}, {Species::O2, -0.5}, {Species::H2O, 1.0}};
  const double dH_J_per_mol = thermo::deltaH(combustion, 298.15);   // negative
  const double M_g_per_mol  = properties(Species::H2).molar_mass;
  // J/mol / (g/mol) = J/g = kJ/kg; /1000 -> MJ/kg
  return -dH_J_per_mol / M_g_per_mol / 1000.0;
}

double pem_efficiency(double P_mod_MW, double p_PEM_bar, const PemEfficiencyPoly& eff) {
  return eff.a00 + eff.a10 * P_mod_MW + eff.a20 * P_mod_MW * P_mod_MW + eff.a01 * p_PEM_bar;
}

ElectrolyzerResult run_electrolyzer(const ElectrolyzerInput& in, const ElectrolyzerConfig& cfg) {
  ElectrolyzerResult r;

  if (cfg.n_modules < 1) {
    r.message = "n_modules must be >= 1";
    return r;
  }
  if (!(in.P_PEM_MW >= 0.0)) {
    r.message = "P_PEM_MW must be non-negative";
    return r;
  }
  if (!(in.p_PEM_bar > 0.0)) {
    r.message = "p_PEM_bar must be positive";
    return r;
  }

  // The fit is per module, so the power is divided before the polynomial is
  // evaluated. A 10 MW island as five 2 MW modules and as twenty 0.5 MW
  // modules are different points on the same curve
  r.P_mod_MW = in.P_PEM_MW / static_cast<double>(cfg.n_modules);

  r.in_fit_range = (r.P_mod_MW >= cfg.P_mod_fit_min_MW && r.P_mod_MW <= cfg.P_mod_fit_max_MW &&
                    in.p_PEM_bar >= cfg.p_PEM_fit_min_bar && in.p_PEM_bar <= cfg.p_PEM_fit_max_bar);

  r.eta_PEM_LHV = pem_efficiency(r.P_mod_MW, in.p_PEM_bar, cfg.eff);

  // A negative or greater-than-unity efficiency is not a marginal
  // extrapolation, it is a first-law violation. Fail rather than return it
  if (!(r.eta_PEM_LHV > 0.0 && r.eta_PEM_LHV < 1.0)) {
    r.message = "efficiency polynomial returned a value outside (0, 1) -- refusing to "
                "propagate a physically impossible efficiency";
    return r;
  }

  const double lhv_MJ_per_kg = pem_lhv_h2_MJ_per_kg();
  if (!(lhv_MJ_per_kg > 0.0)) {
    r.message = "degenerate hydrogen lower heating value";
    return r;
  }

  // P in MW is MJ/s; MJ/s divided by MJ/kg gives kg/s
  r.m_dot_H2_kg_s = r.eta_PEM_LHV * in.P_PEM_MW * cfg.faradaic_efficiency / lhv_MJ_per_kg;

  // Stoichiometry of H2O -> H2 + 1/2 O2, per mole of hydrogen
  const double M_H2  = properties(Species::H2).molar_mass;
  const double M_H2O = properties(Species::H2O).molar_mass;
  const double M_O2  = properties(Species::O2).molar_mass;
  r.m_dot_H2O_kg_s = r.m_dot_H2_kg_s * (M_H2O / M_H2);
  r.m_dot_O2_kg_s  = r.m_dot_H2_kg_s * (0.5 * M_O2 / M_H2);

  // Cooling duty is what the stack does not convert to chemical energy, which
  // follows from the definition of efficiency
  r.P_cooling_MW   = in.P_PEM_MW * (1.0 - r.eta_PEM_LHV);
  r.P_auxiliary_MW = cfg.auxiliary_power_fraction * in.P_PEM_MW;
  r.P_total_MW     = in.P_PEM_MW + r.P_auxiliary_MW;

  r.ok = true;
  r.message = r.in_fit_range
                  ? "ok"
                  : "ok (WARNING: outside Table A.1's fitted window -- efficiency is extrapolated)";
  return r;
}

namespace presets {

ElectrolyzerConfig mucci_pem_module() {
  return ElectrolyzerConfig{};   // defaults already are Mucci Table 2 / Table A.1
}

}  // namespace presets

}  // namespace front_end
