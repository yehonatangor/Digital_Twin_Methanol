// Reaction kinetics and reforming equilibrium. Depends on the species table,
// ideal-gas thermodynamics and the rate laws only; nothing in the reactor
// layer, so this layer builds and tests standalone
//
// SOURCES. Methanol synthesis: Vanden Bussche and Froment (1996) rate form,
// Mignard and Pritchard reparameterisation, constants per Van-Dal Table 3,
// Graaf equilibrium correlations. Reforming: steam and dry reforming
// equilibria, checked against thermochemistry rather than a second
// correlation

#include "lhhw.hpp"
#include "trm_kinetics.hpp"
#include "species.hpp"
#include "thermo.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== reaction kinetics and reforming equilibrium ===\n\n");

  // ---------------------------------------------------------------------
  std::printf("[1] Methanol synthesis equilibrium constants at Van-Dal's 493.15 K\n");
  // Keq2 is the FORWARD shift constant. Van-Dal Eq. (9) prints both signs
  // inverted; the forward form is the only one that makes the RWGS bracket
  // vanish at equilibrium. Pinned by value and by temperature trend
  const double T = 493.15;
  const double K1 = lhhw::Keq1(T);
  const double K2 = lhhw::Keq2(T);
  std::printf("       [INFO] Keq1(493.15) = %.6g, Keq2(493.15) = %.6g\n", K1, K2);
  checkTrue("Keq1 is positive", K1 > 0.0);
  checkTrue("Keq2 is positive", K2 > 0.0);

  // Methanol synthesis is exothermic, so its equilibrium constant must fall
  // with temperature. This is the single most diagnostic check on the sign of
  // the correlation: a transposed sign would reverse it
  checkTrue("Keq1 falls with temperature (synthesis is exothermic)",
            lhhw::Keq1(553.15) < lhhw::Keq1(493.15));
  checkTrue("and keeps falling", lhhw::Keq1(593.15) < lhhw::Keq1(553.15));

  // The forward shift is exothermic, so Keq2 must FALL with temperature. An
  // inverted sign would make it rise and would silently break the RWGS
  // driving force
  check("Keq2 at 493.15 K, literature 130 to 150", K2, 149.48, 0.5);
  checkTrue("Keq2 falls with temperature (forward shift is exothermic)",
            lhhw::Keq2(553.15) < lhhw::Keq2(493.15));
  checkTrue("and keeps falling", lhhw::Keq2(593.15) < lhhw::Keq2(553.15));
  checkTrue("Keq2 stays in the shift-constant band, not near unity", K2 > 100.0);

  // ---------------------------------------------------------------------
  std::printf("\n[2] The equilibrium constants agree in sign with this project's own thermo\n");
  // An independent statement of the same physics from the formation-enthalpy
  // table, so the two cannot both be wrong in the same direction by accident
  const thermo::Reaction meoh = {{Species::CO2, -1.0}, {Species::H2, -3.0},
                                 {Species::CH3OH, 1.0}, {Species::H2O, 1.0}};
  const thermo::Reaction rwgs = {{Species::CO2, -1.0}, {Species::H2, -1.0},
                                 {Species::CO, 1.0}, {Species::H2O, 1.0}};
  checkTrue("thermo agrees synthesis is exothermic", thermo::deltaH(meoh, T) < 0.0);
  checkTrue("thermo agrees RWGS is endothermic",     thermo::deltaH(rwgs, T) > 0.0);
  // The forward shift is the reverse of that reaction, so its constant is the
  // reciprocal and must exceed one at this temperature
  checkTrue("forward shift is favoured at 493 K, as Keq2 reports",
            1.0 / thermo::Keq(rwgs, T) > 1.0);
  checkTrue("thermo's own Keq for synthesis also falls with T",
            thermo::Keq(meoh, 553.15) < thermo::Keq(meoh, 493.15));
  checkTrue("thermo's own Keq for RWGS also rises with T",
            thermo::Keq(rwgs, 553.15) > thermo::Keq(rwgs, 493.15));

  // ---------------------------------------------------------------------
  std::printf("\n[3] LHHW rate laws at a representative synthesis composition\n");
  // Partial pressures in bar, a plausible point inside the loop
  const double P_CO2 = 5.0, P_H2 = 50.0, P_H2O = 1.0, P_CH3OH = 1.0, P_CO = 1.0;
  const double r_meoh = lhhw::r_CH3OH(P_CO2, P_H2, P_H2O, P_CH3OH, T);
  const double r_rwgs = lhhw::r_RWGS(P_CO2, P_H2, P_H2O, P_CO, T);
  std::printf("       [INFO] r_CH3OH = %.6g, r_RWGS = %.6g mol/(kgcat s)\n", r_meoh, r_rwgs);
  checkTrue("methanol forms at this composition", r_meoh > 0.0);
  checkTrue("both rates are finite", std::isfinite(r_meoh) && std::isfinite(r_rwgs));

  // Hydrogen appears to a high power in the numerator, so the rate must be
  // strongly increasing in it
  checkTrue("rate rises steeply with hydrogen partial pressure",
            lhhw::r_CH3OH(P_CO2, 70.0, P_H2O, P_CH3OH, T) > r_meoh);
  checkTrue("rate rises with carbon dioxide partial pressure",
            lhhw::r_CH3OH(8.0, P_H2, P_H2O, P_CH3OH, T) > r_meoh);

  // Water inhibits: it appears in the adsorption denominator. This is the
  // mechanism that makes a CO2 fed loop slower than a CO fed one
  checkTrue("water inhibits the synthesis rate",
            lhhw::r_CH3OH(P_CO2, P_H2, 5.0, P_CH3OH, T) < r_meoh);

  // ---------------------------------------------------------------------
  std::printf("\n[4] The rate laws respect their own equilibrium\n");
  // Driving the product side far past equilibrium must reverse the sign of the
  // methanol rate. A rate law that stayed positive there would let the reactor
  // integrate past equilibrium and manufacture product
  const double r_fwd = lhhw::r_CH3OH(P_CO2, P_H2, 0.1, 0.1, T);
  const double r_rev = lhhw::r_CH3OH(P_CO2, P_H2, 60.0, 60.0, T);
  checkTrue("forward at low product partial pressure", r_fwd > 0.0);
  checkTrue("reverses when products are driven far above equilibrium", r_rev < 0.0);
  checkTrue("RWGS also reverses when its products dominate",
            lhhw::r_RWGS(0.1, 0.1, 60.0, 60.0, T) < 0.0);
  // And runs forward when CO2 and H2 dominate instead
  checkTrue("RWGS runs forward when its reactants dominate",
            lhhw::r_RWGS(20.0, 60.0, 0.1, 0.01, T) > 0.0);

  // ---------------------------------------------------------------------
  std::printf("\n[5] Reforming equilibrium constants\n");
  // Steam and dry reforming are both strongly endothermic, so both constants
  // rise with temperature and are tiny at synthesis temperatures
  checkTrue("Keq_SRM rises with temperature",
            trm_kinetics::Keq_SRM(1100.0) > trm_kinetics::Keq_SRM(900.0));
  checkTrue("Keq_DRM rises with temperature",
            trm_kinetics::Keq_DRM(1100.0) > trm_kinetics::Keq_DRM(900.0));
  checkTrue("reforming is negligible at synthesis temperature",
            trm_kinetics::Keq_SRM(493.15) < 1.0);
  checkTrue("and strongly favoured at reforming temperature",
            trm_kinetics::Keq_SRM(1100.0) > 1.0);

  std::printf("\n[6] Reaction enthalpies carried by the module\n");
  // Both steam reforming values are stated by their own sources; they differ
  // by 500 J/mol, which is within the spread between published tabulations
  check("SRM, Lim 2022, kJ/mol", trm_kinetics::dH_SRM_Lim2022 / 1000.0, 206.3, 1e-9);
  check("SRM, Shi 2020, kJ/mol", trm_kinetics::dH_SRM_Shi2020 / 1000.0, 206.8, 1e-9);
  check("DRM, kJ/mol",           trm_kinetics::dH_DRM / 1000.0,         247.3, 1e-9);
  check("POM, kJ/mol",           trm_kinetics::dH_POM / 1000.0,         -35.6, 1e-9);
  checkTrue("the two SRM tabulations agree to better than 1 kJ/mol",
            std::fabs(trm_kinetics::dH_SRM_Lim2022 - trm_kinetics::dH_SRM_Shi2020) < 1000.0);
  checkTrue("steam reforming is endothermic",   trm_kinetics::dH_SRM > 0.0);
  checkTrue("dry reforming is more endothermic than steam", trm_kinetics::dH_DRM > trm_kinetics::dH_SRM);
  checkTrue("partial oxidation is exothermic",  trm_kinetics::dH_POM < 0.0);

  // Cross-check the tabulated values against this project's own formation
  // data. Agreement to a few kJ/mol is all that should be claimed, since the
  // sources tabulate at slightly different reference states
  const thermo::Reaction srm = {{Species::CH4, -1.0}, {Species::H2O, -1.0},
                                {Species::CO, 1.0}, {Species::H2, 3.0}};
  const thermo::Reaction drm = {{Species::CH4, -1.0}, {Species::CO2, -1.0},
                                {Species::CO, 2.0}, {Species::H2, 2.0}};
  check("SRM against the formation table, kJ/mol",
        thermo::deltaH(srm, 298.15) / 1000.0, 206.3, 3.0);
  check("DRM against the formation table, kJ/mol",
        thermo::deltaH(drm, 298.15) / 1000.0, 247.3, 3.0);

  // ---------------------------------------------------------------------
  std::printf("\n[7] Equilibrium extent solver on a tri-reforming feed\n");
  trm_kinetics::FeedMoles feed;
  feed.CH4 = 1.0; feed.H2O = 1.0; feed.CO2 = 0.4; feed.O2 = 0.1;
  const auto eq = trm_kinetics::solveEquilibrium(feed, 1100.0, 20.0);
  checkTrue("converged", eq.converged);
  std::printf("       [INFO] xi_POM %.4f, xi_SRM %.4f, xi_DRM %.4f\n",
              eq.xi_POM, eq.xi_SRM, eq.xi_DRM);
  checkTrue("methane is consumed", eq.outlet.CH4 < feed.CH4);
  checkTrue("hydrogen is produced", eq.outlet.H2 > feed.H2);
  checkTrue("carbon monoxide is produced", eq.outlet.CO > feed.CO);
  checkTrue("no negative moles leave the solver",
            eq.outlet.CH4 >= 0.0 && eq.outlet.H2O >= 0.0 && eq.outlet.CO2 >= 0.0 &&
            eq.outlet.O2 >= 0.0 && eq.outlet.CO >= 0.0 && eq.outlet.H2 >= 0.0);

  std::printf("\n[8] Atoms are conserved across the equilibrium solve\n");
  auto C = [](const trm_kinetics::FeedMoles& m) { return m.CH4 + m.CO2 + m.CO; };
  auto H = [](const trm_kinetics::FeedMoles& m) { return 4*m.CH4 + 2*m.H2O + 2*m.H2; };
  auto O = [](const trm_kinetics::FeedMoles& m) { return m.H2O + 2*m.CO2 + 2*m.O2 + m.CO; };
  checkRel("carbon",   C(eq.outlet), C(feed), 1e-9);
  checkRel("hydrogen", H(eq.outlet), H(feed), 1e-9);
  checkRel("oxygen",   O(eq.outlet), O(feed), 1e-9);

  std::printf("\n[9] Conversion helpers\n");
  const double xCH4 = trm_kinetics::conversionCH4(feed, eq.outlet);
  checkTrue("methane conversion is a fraction", xCH4 > 0.0 && xCH4 <= 1.0);
  check("matches the direct definition", xCH4,
        (feed.CH4 - eq.outlet.CH4) / feed.CH4, 1e-12);
  std::printf("       [INFO] CH4 conversion %.4f\n", xCH4);

  std::printf("\n[10] Hotter reforming converts more methane\n");
  const auto hot = trm_kinetics::solveEquilibrium(feed, 1200.0, 20.0);
  checkTrue("converged", hot.converged);
  checkTrue("higher temperature converts more methane",
            trm_kinetics::conversionCH4(feed, hot.outlet) >= xCH4 - 1e-9);

  return report("kinetics");
}
