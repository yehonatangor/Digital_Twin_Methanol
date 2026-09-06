// Tri-reforming reactor
//
// Kinetics: Xu & Froment (1989) reactions I, II and III, with the methane
// combustion step from Trimm & Lam. Parameter set and effectiveness factors
// per Aboosadi et al. (2011)

#include "front_end/trm_reactor.hpp"
#include "_harness.inc"

namespace {

void atoms(const reactor::SpeciesArray& F, double& C, double& H, double& O) {
  auto f = [&](Species s) { return F[static_cast<std::size_t>(s)]; };
  C = f(Species::CO2) + f(Species::CO) + f(Species::CH3OH) + f(Species::CH4);
  H = 2.0 * f(Species::H2) + 2.0 * f(Species::H2O) + 4.0 * f(Species::CH3OH)
    + 4.0 * f(Species::CH4);
  O = 2.0 * f(Species::CO2) + f(Species::CO) + f(Species::H2O) + f(Species::CH3OH)
    + 2.0 * f(Species::O2);
}

}  // namespace

int main() {
  std::printf("=== tri-reforming reactor ===\n\n");

  const auto kp = front_end::aboosadi_kinetics_params();

  std::printf("[1] Xu & Froment Table 5 rate constants\n");
  // Reported as A and E; the values here are the ones inside the paper's own
  // stated confidence intervals
  checkTrue("SRM pre-exponential is positive",     kp.k_srm_kmol_h.A > 0.0);
  checkTrue("WGS pre-exponential is positive",     kp.k_wgs_kmol_h.A > 0.0);
  checkTrue("overall pre-exponential is positive", kp.k_overall_kmol_h.A > 0.0);
  check("SRM activation energy, kJ/mol",     kp.k_srm_kmol_h.E_J_mol / 1000.0,     240.1, 0.2);
  check("WGS activation energy, kJ/mol",     kp.k_wgs_kmol_h.E_J_mol / 1000.0,      67.13, 0.2);
  check("overall activation energy, kJ/mol", kp.k_overall_kmol_h.E_J_mol / 1000.0, 243.9, 0.2);

  std::printf("\n[2] Adsorption enthalpies have the right signs\n");
  checkTrue("CO adsorption is exothermic",  kp.K_CO.dH_J_mol  < 0.0);
  checkTrue("H2 adsorption is exothermic",  kp.K_H2.dH_J_mol  < 0.0);
  checkTrue("CH4 adsorption is exothermic", kp.K_CH4.dH_J_mol < 0.0);
  checkTrue("water adsorption is endothermic (Xu & Froment sign)",
            kp.K_H2O.dH_J_mol > 0.0);

  std::printf("\n[3] Stoichiometry is balanced reaction by reaction\n");
  for (int j = 0; j < front_end::N_TRM_RXN; ++j) {
    reactor::SpeciesArray nu{};
    for (int i = 0; i < reactor::NS; ++i) {
      nu[static_cast<std::size_t>(i)] =
          front_end::kTrmStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
    }
    double C, H, O;
    atoms(nu, C, H, O);
    check("carbon closes",   C, 0.0, 1e-12);
    check("hydrogen closes", H, 0.0, 1e-12);
    check("oxygen closes",   O, 0.0, 1e-12);
  }

  std::printf("\n[4] Aboosadi's effectiveness factors\n");
  const front_end::EffectivenessFactors eta;
  check("SRM",        eta.eta_srm,        0.07, 1e-12);
  check("overall",    eta.eta_overall,    0.06, 1e-12);
  check("WGS",        eta.eta_wgs,        0.70, 1e-12);
  check("combustion", eta.eta_combustion, 0.05, 1e-12);

  std::printf("\n[5] The bed geometry is flagged as unsourced\n");
  checkTrue("Aboosadi publishes no tube geometry",
            !front_end::presets::kAboosadiBedGeometrySourced);

  std::printf("\n[6] Aboosadi's optimised feed runs\n");
  front_end::TrmReactorConfig cfg;
  cfg.bed = front_end::presets::aboosadi_tri_reformer_bed();
  cfg.catalyst_mass_total_kg = 5600.0;   // Aboosadi's stated charge
  cfg.record_profile = false;
  const auto inlet = front_end::aboosadi_optimized_feed();
  const auto r = front_end::integrate_trm_reactor(inlet, cfg);
  checkTrue("ok", r.ok);
  std::printf("       [INFO] %s\n", r.message.c_str());
  std::printf("       [INFO] CH4 conversion %.3f, H2/CO %.3f, T_out %.1f K\n",
              r.ch4_conversion(inlet), r.h2_co_ratio(), r.outlet.T_K);
  checkTrue("methane is consumed", r.ch4_conversion(inlet) > 0.0);
  checkTrue("conversion is a fraction", r.ch4_conversion(inlet) <= 1.0);
  checkTrue("syngas is produced", r.h2_co_ratio() > 0.0);
  // Tri-reforming targets an H2/CO ratio near 2 for methanol synthesis
  check("H2/CO ratio", r.h2_co_ratio(), 2.0, 1.2);

  std::printf("\n[7] Atoms are conserved through the tri-reformer\n");
  double Ci, Hi, Oi, Co, Ho, Oo;
  atoms(inlet.F, Ci, Hi, Oi);
  atoms(r.outlet.F, Co, Ho, Oo);
  checkRel("carbon",   Co, Ci, 1e-10);
  checkRel("hydrogen", Ho, Hi, 1e-10);
  checkRel("oxygen",   Oo, Oi, 1e-10);

  std::printf("\n[8] The clamp absorbed nothing\n");
  check("largest clamp excursion", r.max_clamp_rel, 0.0, 1e-12);

  std::printf("\n[9] Reaction enthalpies have the expected signs\n");
  checkTrue("steam reforming is endothermic",
            front_end::trm_delta_H_rxn_J_per_mol(front_end::RXN_SRM, 1100.0) > 0.0);
  checkTrue("combustion is exothermic",
            front_end::trm_delta_H_rxn_J_per_mol(front_end::RXN_COMBUSTION, 1100.0) < 0.0);

  return report("tri-reforming reactor");
}
