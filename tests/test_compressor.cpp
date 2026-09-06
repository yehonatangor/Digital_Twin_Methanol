// Hydrogen compressor against Mucci et al. (2023) Table A.2
//
// Table A.2 prints shaft power at several pressure ratios. Two of those cases
// are reproduced here from the paper's own stated inputs

#include "front_end/compressor.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== hydrogen compressor (Mucci Sec. 3.3) ===\n\n");

  const auto cfg = front_end::presets::mucci_h2_compressor();

  std::printf("[1] Sec. 3.3 operating assumptions\n");
  check("inlet temperature, K", cfg.T_in_K,        298.15, 1e-12);
  check("isentropic efficiency", cfg.eta_isentropic, 0.8,  1e-12);
  check("mechanical efficiency", cfg.eta_mechanical, 0.877, 1e-3);
  checkTrue("the mechanical efficiency is flagged as derived, not printed",
            !cfg.eta_mechanical_sourced);

  std::printf("\n[2] Table A.2, beta = 2.0 and beta = 3.5\n");
  const auto r2 = front_end::run_compressor(front_end::presets::mucci_table_a2_case(2.0), cfg);
  checkTrue("ok", r2.ok);
  check("beta", r2.beta, 2.0, 1e-9);
  check("shaft power, kW", r2.P_comp_MW * 1000.0, 710.0, 2.0);

  const auto r35 = front_end::run_compressor(front_end::presets::mucci_table_a2_case(3.5), cfg);
  checkTrue("ok", r35.ok);
  check("beta", r35.beta, 3.5, 1e-9);
  check("shaft power, kW", r35.P_comp_MW * 1000.0, 1397.0, 3.0);

  std::printf("\n[3] cp and k come from the project's own thermo, not a literal\n");
  checkTrue("cp is a physical hydrogen value", r2.cp_J_per_kgK > 13000.0 && r2.cp_J_per_kgK < 16000.0);
  checkTrue("k is between 1 and 2", r2.k_isentropic_exponent > 1.0 && r2.k_isentropic_exponent < 2.0);
  check("k = cp / (cp - R), Mayer relation", r2.k_isentropic_exponent,
        r2.cp_J_per_kgK / (r2.cp_J_per_kgK - 8.314462618 / (2.016e-3)), 1e-6);

  std::printf("\n[4] Cooling equals shaft power, per Mucci\n");
  check("cooling duty", r2.P_cooling_MW, r2.P_comp_MW, 1e-12);

  std::printf("\n[5] Monotonicity and guards\n");
  checkTrue("more compression costs more power", r35.P_comp_MW > r2.P_comp_MW);
  checkTrue("a pressure ratio below 1 is rejected",
            !front_end::run_compressor({1.0, 78.0, 30.0}, cfg).ok);
  // Zero flow is a valid idle compressor, not an error: the dispatch loop
  // calls this on hours when nothing is running
  const auto idle = front_end::run_compressor({0.0, 30.0, 78.0}, cfg);
  checkTrue("zero flow is accepted as an idle unit", idle.ok);
  check("and draws no power", idle.P_comp_MW, 0.0, 1e-12);
  checkTrue("negative flow is rejected",
            !front_end::run_compressor({-1.0, 30.0, 78.0}, cfg).ok);

  return report("compressor");
}
