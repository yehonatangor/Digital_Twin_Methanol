// Ideal-gas thermodynamics and the species table
//
// Reference values come from tests/reference_data.hpp, which labels each one
// INDEPENDENT or TRANSCRIPTION. The reaction enthalpies below are the
// independent ones: three papers publish them and none of them supplied this
// project's formation table, so agreement tests the table, the Kirchhoff
// integration and the stoichiometry together

#include "species.hpp"
#include "thermo.hpp"
#include "_harness.inc"
#include "reference_data.hpp"

int main() {
  std::printf("=== thermodynamics ===\n\n");

  std::printf("[1] Species table against CRC and NIST, all 9 species\n");
  // TRANSCRIPTION: species.cpp cites the same source, so this catches a
  // mistyped digit and claims nothing more
  char lbl[96];
  for (const auto& r : refdata::kProperties) {
    const Properties& p = properties(r.sp);
    std::snprintf(lbl, sizeof lbl, "%s molar mass, g/mol", speciesName(r.sp));
    check(lbl, p.molar_mass, r.M_g_mol, 0.002);
    std::snprintf(lbl, sizeof lbl, "%s Tc, K", speciesName(r.sp));
    check(lbl, p.Tc, r.Tc_K, 0.05);
    std::snprintf(lbl, sizeof lbl, "%s Pc, bar", speciesName(r.sp));
    check(lbl, p.Pc / 1e5, r.Pc_Pa / 1e5, 0.01);
    std::snprintf(lbl, sizeof lbl, "%s omega", speciesName(r.sp));
    check(lbl, p.omega, r.omega, 1e-9);
  }
  // Methanol's acentric factor exceeds 0.49, so the 1978 kappa branch is
  // required. A model stuck on the 1976 branch would misprice it
  checkTrue("methanol omega > 0.49 (needs the PR-1978 branch)",
            properties(Species::CH3OH).omega > 0.49);

  std::printf("\n[2] Formation enthalpy and entropy, all 9 species\n");
  // TRANSCRIPTION, same reason. deltaH at the reference temperature must return
  // the tabulated formation enthalpy exactly, which also pins T_ref
  for (const auto& r : refdata::kFormation) {
    const thermo::Reaction form = {{r.sp, 1.0}};
    std::snprintf(lbl, sizeof lbl, "%s dHf298, kJ/mol", speciesName(r.sp));
    check(lbl, thermo::deltaH(form, 298.15) / 1000.0, r.dHf298_kJ_mol, 0.01);
    std::snprintf(lbl, sizeof lbl, "%s S298, J/(mol K)", speciesName(r.sp));
    check(lbl, thermo::entropy(r.sp, 298.15), r.S298_J_mol_K, 0.01);
  }

  std::printf("\n[3] Heat capacity is positive and rises with temperature\n");
  // No independent Cp reference exists: species.cpp takes DIPPR-107 from
  // Perry's Table 2-155 and this project has no second source for Cp. Only
  // properties that hold regardless of the coefficients are asserted
  for (const auto& r : refdata::kProperties) {
    std::snprintf(lbl, sizeof lbl, "cp(%s) > 0 at 298 K", speciesName(r.sp));
    checkTrue(lbl, thermo::cp(r.sp, 298.15) > 0.0);
  }
  checkTrue("cp(CO2) rises from 300 to 800 K",
            thermo::cp(Species::CO2, 800.0) > thermo::cp(Species::CO2, 300.0));
  checkTrue("monatomic argon cp is flat in temperature",
            std::fabs(thermo::cp(Species::Ar, 800.0)
                      - thermo::cp(Species::Ar, 300.0)) < 1e-6);

  std::printf("\n[4] Reaction enthalpies against published values\n");
  // INDEPENDENT. Tolerances are the spread across the citing papers
  const thermo::Reaction meoh = {{Species::CO2, -1.0}, {Species::H2, -3.0},
                                 {Species::CH3OH, 1.0}, {Species::H2O, 1.0}};
  const thermo::Reaction rwgs = {{Species::CO2, -1.0}, {Species::H2, -1.0},
                                 {Species::CO, 1.0}, {Species::H2O, 1.0}};
  const thermo::Reaction srm = {{Species::CH4, -1.0}, {Species::H2O, -1.0},
                                {Species::CO, 1.0}, {Species::H2, 3.0}};
  const thermo::Reaction drm = {{Species::CH4, -1.0}, {Species::CO2, -1.0},
                                {Species::CO, 2.0}, {Species::H2, 2.0}};
  const thermo::Reaction pox = {{Species::CH4, -1.0}, {Species::O2, -0.5},
                                {Species::CO, 1.0}, {Species::H2, 2.0}};

  check(refdata::kMethanolSynthesis.name, thermo::deltaH(meoh, 298.15) / 1000.0,
        refdata::kMethanolSynthesis.dH298_kJ_mol,
        refdata::kMethanolSynthesis.tol_kJ_mol);
  check(refdata::kReverseShift.name, thermo::deltaH(rwgs, 298.15) / 1000.0,
        refdata::kReverseShift.dH298_kJ_mol, refdata::kReverseShift.tol_kJ_mol);
  check(refdata::kSteamReforming.name, thermo::deltaH(srm, 298.15) / 1000.0,
        refdata::kSteamReforming.dH298_kJ_mol,
        refdata::kSteamReforming.tol_kJ_mol);
  check(refdata::kDryReforming.name, thermo::deltaH(drm, 298.15) / 1000.0,
        refdata::kDryReforming.dH298_kJ_mol, refdata::kDryReforming.tol_kJ_mol);
  check(refdata::kPartialOxidation.name, thermo::deltaH(pox, 298.15) / 1000.0,
        refdata::kPartialOxidation.dH298_kJ_mol,
        refdata::kPartialOxidation.tol_kJ_mol);

  std::printf("\n[5] Equilibrium constants move the right way\n");
  checkTrue("methanol synthesis Keq falls as T rises (exothermic)",
            thermo::Keq(meoh, 550.0) < thermo::Keq(meoh, 450.0));
  checkTrue("RWGS Keq rises as T rises (endothermic)",
            thermo::Keq(rwgs, 800.0) > thermo::Keq(rwgs, 500.0));
  check("lnKeq and Keq agree", std::log(thermo::Keq(meoh, 500.0)),
        thermo::lnKeq(meoh, 500.0), 1e-9);

  std::printf("\n[6] Gibbs energy is consistent with H and S\n");
  const double T = 500.0;
  check("dG = dH - T dS", thermo::deltaG(meoh, T),
        thermo::deltaH(meoh, T) - T * thermo::deltaS(meoh, T), 1.0);

  std::printf("\n[7] Hydrogen LHV, derived rather than tabulated separately\n");
  // H2 + 0.5 O2 -> H2O(g). Handbook LHV is 119.96 MJ/kg. Computed here from
  // the formation-enthalpy table directly rather than by calling the
  // electrolyser module, so this stage's tests depend on nothing later
  const thermo::Reaction h2_comb = {
      {Species::H2, -1.0}, {Species::O2, -0.5}, {Species::H2O, 1.0}};
  const double lhv_h2 = -thermo::deltaH(h2_comb, 298.15)
                        / properties(Species::H2).molar_mass / 1000.0;
  check("LHV, MJ/kg", lhv_h2, refdata::kH2LowerHeatingValue_MJ_kg, 0.05);

  std::printf("\n[8] Electrolysis stoichiometric mass ratios\n");
  // H2O -> H2 + 0.5 O2, so per kg of H2: 8.937 kg water in, 7.937 kg oxygen out
  const double M_H2 = properties(Species::H2).molar_mass;
  const double M_H2O = properties(Species::H2O).molar_mass;
  const double M_O2 = properties(Species::O2).molar_mass;
  check("water per kg H2",  M_H2O / M_H2,        8.937, 0.002);
  check("oxygen per kg H2", 0.5 * M_O2 / M_H2,   7.937, 0.002);

  std::printf("\n[9] Vapour pressure, DIPPR-101\n");
  // INDEPENDENT. A normal boiling point is a different fact about the substance
  // than the DIPPR-101 coefficients, so requiring 1 atm there is a real check
  for (const auto& b : refdata::kNormalBoiling) {
    std::snprintf(lbl, sizeof lbl, "%s Psat at its normal boiling point, bar",
                  speciesName(b.sp));
    check(lbl, vaporPressure(b.sp, b.T_boil_K) / 1e5, 1.01325, 0.03);
  }
  checkTrue("methanol is more volatile than water at 330 K",
            vaporPressure(Species::CH3OH, 330.0) > vaporPressure(Species::H2O, 330.0));
  checkTrue("permanent gases carry no DIPPR-101 data",
            vaporPressureParams(Species::H2) == nullptr);

  return report("thermodynamics");
}
