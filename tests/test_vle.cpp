// Methanol/water VLE against measured data
//
// Guards the NRTL pair ORIENTATION, not just its digits. NRTL is asymmetric
// (tau_12 != tau_21); swapping the two halves does not crash or NaN, it just
// exchanges the infinite-dilution activity coefficients and produces a
// plausible but wrong separation. Measured, and what calibrates the
// tolerances below:
//
//     x_MeOH   y correct   y swapped   literature
//     0.02      0.1328      0.1068      0.134
//     0.10      0.4244      0.3908      0.418
//     0.50      0.7856      0.8110      0.779
//     gamma_inf(MeOH in water): 2.21 correct, 1.68 swapped
//
// Correct sits within 0.008 of literature; the swap misses by 0.027-0.032 in
// the dilute region. Tolerances sit between those bands
//
// REFERENCE DATA: methanol/water at 101.325 kPa, Gmehling & Onken, DECHEMA
// Vol. I. Published datasets differ by about +/-0.005 in y, which the
// tolerances accommodate
//
// Also exercises DIPPR-101 Psat for both species, since a bubble point is
// sum_i x_i gamma_i Psat_i(T) = P

#include "nrtl.hpp"
#include "species.hpp"
#include "stream.hpp"
#include "units.hpp"
#include "reference_data.hpp"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(const char* what, double got, double want, double abs_tol) {
  const bool ok = std::fabs(got - want) <= abs_tol;
  if (!ok) ++failures;
  std::printf("[%s] %-52s got %9.4f  want %9.4f  (+/-%.3f)\n",
              ok ? "PASS" : "FAIL", what, got, want, abs_tol);
}

void checkTrue(const char* what, bool cond) {
  if (!cond) ++failures;
  std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", what);
}

// Bubble-point solve: find T such that sum_i x_i*gamma_i(T)*Psat_i(T) = P
// The test's own independent statement of the equilibrium condition, so it
// cannot agree with the model through a shared bug. Bisection: monotonic in T
double bubblePointK(double x_meoh, double P_Pa, double& y_meoh_out) {
  double lo = 300.0, hi = 400.0;
  double y = 0.0;
  for (int it = 0; it < 200; ++it) {
    const double T = 0.5 * (lo + hi);

    Stream liq;
    liq.temperature = T;
    liq.pressure = P_Pa;
    liq.phase = Phase::Liquid;
    liq.molar_flow[Species::CH3OH] = x_meoh;
    liq.molar_flow[Species::H2O] = 1.0 - x_meoh;

    const auto gamma = nrtl::activityCoefficients(liq, T);
    const double pM = x_meoh * gamma.at(Species::CH3OH) * vaporPressure(Species::CH3OH, T);
    const double pW = (1.0 - x_meoh) * gamma.at(Species::H2O) * vaporPressure(Species::H2O, T);
    const double total = pM + pW;

    y = (total > 0.0) ? pM / total : 0.0;
    if (total > P_Pa) hi = T; else lo = T;
  }
  y_meoh_out = y;
  return 0.5 * (lo + hi);
}

// Infinite-dilution activity coefficient of `solute` in `solvent`
double gammaInfinite(Species solute, Species solvent, double T_K) {
  Stream liq;
  liq.temperature = T_K;
  liq.phase = Phase::Liquid;
  liq.molar_flow[solute] = 1.0e-8;
  liq.molar_flow[solvent] = 1.0;
  return nrtl::activityCoefficients(liq, T_K).at(solute);
}

// -----------------------------------------------------------------------------
// [1] Orientation contract. ChemSep stores tau(MeOH->H2O) = -95.132/T and
// tau(H2O->MeOH) = +398.953/T. A swap flips both signs and fails here first
// -----------------------------------------------------------------------------
void testOrientationContract() {
  std::printf("\n[1] NRTL orientation contract (ChemSep methanol/water)\n");

  const double T = 337.0;
  const double tau_MW = nrtl::tau(Species::CH3OH, Species::H2O, T);
  const double tau_WM = nrtl::tau(Species::H2O, Species::CH3OH, T);

  check("tau(CH3OH -> H2O) = -95.13209.../T", tau_MW, -95.13209282738782 / T, 1e-9);
  check("tau(H2O -> CH3OH) = +398.95345.../T", tau_WM, 398.95345259688855 / T, 1e-9);
  checkTrue("CONTRACT: tau(MeOH->H2O) is NEGATIVE", tau_MW < 0.0);
  checkTrue("CONTRACT: tau(H2O->MeOH) is POSITIVE", tau_WM > 0.0);
  checkTrue("CONTRACT: the two directions are not equal (NRTL is asymmetric)",
            std::fabs(tau_MW - tau_WM) > 1.0);

  // Symmetry that SHOULD hold: alpha is shared, and G follows from tau
  check("G(i,j) = exp(-alpha*tau(i,j))", nrtl::G(Species::CH3OH, Species::H2O, T),
        std::exp(-0.2999 * tau_MW), 1e-12);
  checkTrue("self-pair is ideal: tau(i,i) = 0", nrtl::tau(Species::CH3OH, Species::CH3OH, T) == 0.0);
  checkTrue("self-pair is ideal: G(i,i) = 1", nrtl::G(Species::H2O, Species::H2O, T) == 1.0);

  checkTrue("has_binary_params reports the methanol/water pair as fitted",
            nrtl::has_binary_params(Species::CH3OH, Species::H2O));
  checkTrue("has_binary_params reports CO2/methanol as NOT fitted (ChemSep has no CO2)",
            !nrtl::has_binary_params(Species::CO2, Species::CH3OH));
}

// -----------------------------------------------------------------------------
// [2] Pure-component endpoints -- validates the DIPPR-101 vapour pressures
// -----------------------------------------------------------------------------
void testEndpoints() {
  std::printf("\n[2] Pure-component boiling points at 1 atm (DIPPR-101 check)\n");

  double y = 0.0;
  const double T_water = bubblePointK(1.0e-9, units::PA_PER_ATM, y);
  const double T_meoh  = bubblePointK(1.0 - 1.0e-9, units::PA_PER_ATM, y);

  check("normal boiling point of water  [degC]", T_water - 273.15, 100.0, 0.5);
  check("normal boiling point of methanol [degC]", T_meoh - 273.15, 64.7, 0.6);
  checkTrue("methanol boils below water (correct volatility order)", T_meoh < T_water);
}

// -----------------------------------------------------------------------------
// [3] Isobaric bubble curve. Points chosen for DISCRIMINATION: the dilute
// region separates the orientations, x > 0.7 is only a shape check
// -----------------------------------------------------------------------------
void testBubbleCurve() {
  std::printf("\n[3] Methanol/water bubble curve at 101.325 kPa vs DECHEMA data\n");

  char label[96];
  double prevT = 1.0e9;
  for (const auto& p : refdata::kMeohWaterBubble) {
    double y = 0.0;
    const double T_C = bubblePointK(p.x_meoh, units::PA_PER_ATM, y) - 273.15;

    std::snprintf(label, sizeof(label), "y_MeOH at x = %.2f", p.x_meoh);
    check(label, y, p.y_meoh, 0.015);          // swap misses by 0.027-0.032 here
    std::snprintf(label, sizeof(label), "T_bubble at x = %.2f [degC]", p.x_meoh);
    check(label, T_C, p.T_C, 1.0);        // swap misses by ~1.7 K at x=0.10

    checkTrue("bubble temperature decreases monotonically with x_MeOH", T_C < prevT);
    prevT = T_C;
    checkTrue("no azeotrope: vapour is always methanol-enriched (y > x)", y > p.x_meoh);
  }
}

// -----------------------------------------------------------------------------
// [4] Infinite-dilution activity coefficients. Sharpest discriminator: a swap
// exchanges the two, so checking BOTH pins the assignment
// -----------------------------------------------------------------------------
void testInfiniteDilution() {
  std::printf("\n[4] Infinite-dilution activity coefficients\n");

  const double T = 337.0;
  const double g_M_in_W = gammaInfinite(Species::CH3OH, Species::H2O, T);
  const double g_W_in_M = gammaInfinite(Species::H2O, Species::CH3OH, T);

  std::printf("       [INFO] gamma_inf(MeOH in water) = %.4f, gamma_inf(water in MeOH) = %.4f\n",
              g_M_in_W, g_W_in_M);

  // Both must exceed 1 -- methanol/water is a positive-deviation system
  // This IS a literature fact, not a regression pin
  checkTrue("methanol/water shows positive deviation (both gamma_inf > 1)",
            g_M_in_W > 1.0 && g_W_in_M > 1.0);

  // -- Literature band check ------------------------------------------------
  // Reported gamma_inf for water in methanol clusters around 1.6-1.9 near the
  // normal boiling range; for methanol in water, published values are more
  // scattered (roughly 1.6-2.5 depending on temperature and method). The bands
  // below are deliberately wide because that scatter is real -- they test that
  // the model lands in the measured region, not that it hits one number
  check("gamma_inf(water in MeOH) inside the literature band", g_W_in_M, 1.75, 0.35);
  check("gamma_inf(MeOH in water) inside the literature band", g_M_in_W, 2.05, 0.55);

  // -- Regression pins ------------------------------------------------------
  // LABELLED HONESTLY: these two numbers are this implementation's own output,
  // not measured values. They exist to catch accidental future drift, and they
  // prove nothing about correctness on their own -- the bubble curve in [3] is
  // what checks this model against real data. Stated explicitly because a pin
  // dressed up as a literature comparison is exactly the failure mode
  // reference_data.hpp was written to avoid
  check("REGRESSION PIN: gamma_inf(MeOH in water)", g_M_in_W, 2.4027, 0.01);
  check("REGRESSION PIN: gamma_inf(water in MeOH)", g_W_in_M, 1.7294, 0.01);

  // The ordering itself is the orientation fingerprint
  checkTrue("CONTRACT: gamma_inf(MeOH in water) > gamma_inf(water in MeOH) "
            "-- reversed means the binary parameters have been swapped",
            g_M_in_W > g_W_in_M);

  // Ideal-dilution limit: gamma -> 1 as the component becomes pure
  Stream nearly_pure;
  nearly_pure.molar_flow[Species::CH3OH] = 1.0;
  nearly_pure.molar_flow[Species::H2O] = 1.0e-8;
  check("gamma(MeOH) -> 1 in nearly pure methanol",
        nrtl::activityCoefficients(nearly_pure, T).at(Species::CH3OH), 1.0, 1e-4);
}

// -----------------------------------------------------------------------------
// [5] Unparameterised pairs behave as ideal, and say so
// -----------------------------------------------------------------------------
void testUnparameterised() {
  std::printf("\n[5] Unparameterised pairs -- ideal fallback, honestly reported\n");

  Stream s;
  s.molar_flow[Species::CH3OH] = 0.4;
  s.molar_flow[Species::H2O]   = 0.4;
  s.molar_flow[Species::CO2]   = 0.2;   // ChemSep has no CO2 NRTL data at all

  // CO2/MeOH and CO2/H2O are both missing -> 2 unparameterised pairs
  check("unparameterised_pairs() counts the two CO2 pairs",
        nrtl::unparameterised_pairs(s), 2.0, 0.0);

  Stream clean;
  clean.molar_flow[Species::CH3OH] = 0.5;
  clean.molar_flow[Species::H2O]   = 0.5;
  check("a fully parameterised stream reports zero",
        nrtl::unparameterised_pairs(clean), 0.0, 0.0);

  check("tau = 0 for an unparameterised pair (ideal contribution)",
        nrtl::tau(Species::CO2, Species::CH3OH, 320.0), 0.0, 0.0);
  check("G = 1 for an unparameterised pair",
        nrtl::G(Species::CO2, Species::CH3OH, 320.0), 1.0, 1e-12);
}

}  // namespace

int main() {
  std::printf("=== methanol_twin VLE validation (NRTL + DIPPR-101) ===\n");
  testOrientationContract();
  testEndpoints();
  testBubbleCurve();
  testInfiniteDilution();
  testUnparameterised();
  std::printf("\n%s -- %d failure(s)\n", failures ? "FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}
