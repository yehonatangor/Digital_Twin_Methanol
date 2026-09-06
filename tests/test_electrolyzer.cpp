// PEM electrolyser, Mucci et al. (2023) Table 2 and Table A.1

#include "front_end/electrolyzer.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== PEM electrolyser ===\n\n");

  const auto cfg = front_end::presets::mucci_pem_module();

  std::printf("[1] Table A.1 efficiency polynomial coefficients\n");
  check("a00", cfg.eff.a00,  0.813,     1e-12);
  check("a10", cfg.eff.a10, -1.010e-1,  1e-15);
  check("a20", cfg.eff.a20,  1.397e-2,  1e-15);
  check("a01", cfg.eff.a01, -3.118e-4,  1e-18);

  std::printf("\n[2] Table 2 module ratings and the fitted window\n");
  check("nominal power, MW", cfg.module_nominal_power_MW, 2.0, 1e-12);
  check("max power, MW",     cfg.module_max_power_MW,     2.5, 1e-12);
  check("fit window, MW",    cfg.P_mod_fit_min_MW,        0.2, 1e-12);
  check("fit window, bar",   cfg.p_PEM_fit_min_bar,      20.0, 1e-12);
  check("auxiliary fraction", cfg.auxiliary_power_fraction, 0.05, 1e-12);

  std::printf("\n[3] Efficiency is in a physical range and falls with load\n");
  const double e_low  = front_end::pem_efficiency(0.5, 30.0, cfg.eff);
  const double e_high = front_end::pem_efficiency(2.5, 30.0, cfg.eff);
  checkTrue("efficiency at part load is between 0 and 1", e_low > 0.0 && e_low < 1.0);
  checkTrue("efficiency at full load is between 0 and 1", e_high > 0.0 && e_high < 1.0);
  checkTrue("efficiency falls as load rises", e_high < e_low);
  checkTrue("pressure lowers efficiency slightly (a01 < 0)",
            front_end::pem_efficiency(2.0, 40.0, cfg.eff) <
            front_end::pem_efficiency(2.0, 20.0, cfg.eff));

  std::printf("\n[4] A nominal run closes its own energy and mass balances\n");
  const front_end::ElectrolyzerInput in{2.0, 30.0};
  const auto r = front_end::run_electrolyzer(in, cfg);
  checkTrue("ok", r.ok);
  checkTrue("inside the fitted window", r.in_fit_range);
  check("module power = total / n_modules", r.P_mod_MW, 2.0, 1e-12);
  checkTrue("hydrogen is produced", r.m_dot_H2_kg_s > 0.0);
  check("auxiliary is 5 % of P_PEM", r.P_auxiliary_MW, 0.05 * 2.0, 1e-9);
  check("total draw = P_PEM + auxiliary", r.P_total_MW, 2.0 + 0.05 * 2.0, 1e-9);
  check("cooling is the energy-balance remainder",
        r.P_cooling_MW, 2.0 * (1.0 - r.eta_PEM_LHV), 1e-6);
  check("LHV closes the hydrogen balance",
        r.m_dot_H2_kg_s * front_end::pem_lhv_h2_MJ_per_kg(), 2.0 * r.eta_PEM_LHV, 1e-6);

  std::printf("\n[5] Stoichiometric water and oxygen\n");
  check("water per kg H2",  r.m_dot_H2O_kg_s / r.m_dot_H2_kg_s, 8.937, 0.002);
  check("oxygen per kg H2", r.m_dot_O2_kg_s  / r.m_dot_H2_kg_s, 7.937, 0.002);

  std::printf("\n[6] Outside the fitted window the run is flagged, not refused\n");
  const auto hot = front_end::run_electrolyzer({4.0, 30.0}, cfg);
  checkTrue("still evaluates", hot.ok);
  checkTrue("but is flagged as extrapolation", !hot.in_fit_range);
  const auto hp = front_end::run_electrolyzer({2.0, 60.0}, cfg);
  checkTrue("pressure outside 20-40 bar is flagged", !hp.in_fit_range);

  return report("electrolyser");
}
