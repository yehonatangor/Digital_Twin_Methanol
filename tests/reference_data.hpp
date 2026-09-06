#pragma once
// Reference values for the test suite, in one place with their citations
//
// Two kinds, labelled, because the distinction is the whole point:
//
//   INDEPENDENT: the reference comes from a source that did not supply the
//   correlation under test, so agreement is evidence the correlation is right
//
//   TRANSCRIPTION: the code's table and the reference share a source, so
//   agreement shows only that the numbers were copied correctly. Useful, and
//   not the same thing as validation
//
// No ideal-gas Cp reference is held here. The comparison against NIST was done
// by hand and only its per-cell deviations were kept, in docs/03, so there are
// no absolute values to assert. See docs/24-validation.md

#include "species.hpp"

namespace refdata {

// -----------------------------------------------------------------------------
// INDEPENDENT. Methanol and water bubble curve at 101.325 kPa
// Gmehling & Onken, DECHEMA Chemistry Data Series Vol. I, isobaric, 1 atm
// Checks NRTL activity coefficients and DIPPR-101 vapour pressure together,
// neither of which came from DECHEMA. Published sets differ by about 0.005 in
// y, which the test tolerances accommodate
// -----------------------------------------------------------------------------
struct BubblePoint { double x_meoh; double y_meoh; double T_C; };

inline constexpr BubblePoint kMeohWaterBubble[] = {
    {0.02, 0.134, 96.4}, {0.05, 0.267, 92.9}, {0.10, 0.418, 87.7},
    {0.20, 0.579, 81.7}, {0.30, 0.665, 78.0}, {0.40, 0.729, 75.3},
    {0.50, 0.779, 73.1}, {0.60, 0.825, 71.2}, {0.70, 0.870, 69.3},
    {0.80, 0.915, 67.6}, {0.90, 0.958, 66.0},
};

// -----------------------------------------------------------------------------
// INDEPENDENT. Reaction enthalpies at 298.15 K, gas basis, in kJ/mol, from
// papers that did not supply this project's formation table. Computing them
// from formation enthalpy and comparing against a published figure tests the
// table, the Kirchhoff integration and the stoichiometry at once
// Sources: Lim et al. (2022), Shi et al. (2020) and Van-Dal & Bouallou (2013),
// tabulated in docs/04-ideal-gas-thermodynamics.md
// -----------------------------------------------------------------------------
struct ReactionRef {
  const char* name;
  double dH298_kJ_mol;
  double tol_kJ_mol;      // spread across the cited papers, not a fudge factor
  const char* source;
};

inline constexpr ReactionRef kSteamReforming{
    "CH4 + H2O -> CO + 3 H2", 206.3, 0.6, "Lim (2022); Shi (2020) gives 206.8"};
inline constexpr ReactionRef kDryReforming{
    "CH4 + CO2 -> 2 CO + 2 H2", 247.3, 0.6, "Lim (2022) and Shi (2020) agree"};
inline constexpr ReactionRef kPartialOxidation{
    "CH4 + 0.5 O2 -> CO + 2 H2", -35.6, 0.6, "Lim (2022) and Shi (2020) agree"};
inline constexpr ReactionRef kMethanolSynthesis{
    "CO2 + 3 H2 -> CH3OH + H2O", -49.0, 1.0, "Shi (2020), gas basis"};
inline constexpr ReactionRef kReverseShift{
    "CO2 + H2 -> CO + H2O", 41.0, 1.0, "Van-Dal & Bouallou (2013)"};

// Stoichiometry is written out at each call site rather than returned from a
// helper here: thermo::Reaction is std::initializer_list<Term>, whose backing
// array dies at the end of the full expression, so a function returning one
// would hand back a dangling reference

// -----------------------------------------------------------------------------
// TRANSCRIPTION. Molar mass, critical constants and acentric factor
// CRC Handbook / NIST, which is what species.cpp itself cites, so this catches
// a mistyped digit and nothing more. Methanol Tc and Pc are the IUPAC
// recommended values; see docs/03-species-properties.md
// -----------------------------------------------------------------------------
struct PropertyRef { Species sp; double M_g_mol, Tc_K, Pc_Pa, omega; };

inline constexpr PropertyRef kProperties[] = {
    {Species::CO2,   44.010, 304.21,  7.383e6,  0.224},
    {Species::H2,     2.016,  33.19,  1.313e6, -0.216},
    {Species::CO,    28.010, 132.92,  3.499e6,  0.048},
    {Species::H2O,   18.015, 647.10, 22.064e6,  0.345},
    {Species::CH3OH, 32.042, 512.50,  8.084e6,  0.566},
    {Species::CH4,   16.043, 190.56,  4.599e6,  0.012},
    {Species::N2,    28.013, 126.20,  3.400e6,  0.038},
    {Species::Ar,    39.948, 150.86,  4.898e6, -0.004},
    {Species::O2,    31.999, 154.58,  5.043e6,  0.022},
};

// -----------------------------------------------------------------------------
// TRANSCRIPTION. Formation enthalpy and standard entropy at 298.15 K, ideal
// gas. NIST-JANAF / CRC Handbook, the same source thermo.cpp cites, tabulated
// in docs/04. Methane's formation enthalpy appears as both -74.60 and -74.85
// kJ/mol in the literature; -74.60 is the value carried here and in the code
// -----------------------------------------------------------------------------
struct FormationRef { Species sp; double dHf298_kJ_mol, S298_J_mol_K; };

inline constexpr FormationRef kFormation[] = {
    {Species::CO2,   -393.51, 213.78},
    {Species::H2,       0.00, 130.68},
    {Species::CO,    -110.53, 197.66},
    {Species::H2O,   -241.83, 188.84},
    {Species::CH3OH, -201.00, 239.90},
    {Species::CH4,    -74.60, 186.25},
    {Species::N2,       0.00, 191.60},
    {Species::Ar,       0.00, 154.85},
    {Species::O2,       0.00, 205.15},
};

// -----------------------------------------------------------------------------
// INDEPENDENT. Vapour pressure at the normal boiling point. Both species boil
// at 1 atm by definition of the boiling point, so DIPPR-101 must return
// 101.325 kPa there. The boiling points are handbook values, not DIPPR
// parameters, so this is a check against a different fact about the substance
// -----------------------------------------------------------------------------
struct BoilingRef { Species sp; double T_boil_K; const char* note; };

inline constexpr BoilingRef kNormalBoiling[] = {
    {Species::H2O,   373.15, "water, by definition of the Celsius scale"},
    {Species::CH3OH, 337.70, "methanol, CRC Handbook"},
};

// -----------------------------------------------------------------------------
// INDEPENDENT. Low-pressure vapour viscosity, Pa s. The code evaluates
// DIPPR-102 with Perry's 8th ed. Table 2-312 coefficients; these reference
// values are CRC and NIST figures for the same species at the same
// temperature, so they come from outside Perry's. Tabulated in
// docs/10-transport-properties.md with the deviations
//
// Tolerances are the measured deviation rounded up, not a target. Water is the
// loosest at 2.24 percent because Perry's row for water is a bare
// two-parameter power law with C = D = 0
// -----------------------------------------------------------------------------
struct ViscosityRef { Species sp; double T_K, mu_Pa_s, rel_tol; };

inline constexpr ViscosityRef kVaporViscosity[] = {
    {Species::CO2,   300.00, 1.500e-5, 0.010},
    {Species::H2,    300.00, 8.950e-6, 0.010},
    {Species::CO,    300.00, 1.780e-5, 0.010},
    {Species::H2O,   373.15, 1.230e-5, 0.030},
    {Species::CH3OH, 400.00, 1.310e-5, 0.010},
    {Species::CH4,   300.00, 1.120e-5, 0.010},
    {Species::N2,    300.00, 1.790e-5, 0.010},
    {Species::Ar,    300.00, 2.270e-5, 0.010},
    {Species::O2,    300.00, 2.070e-5, 0.010},
};

// Hydrogen lower heating value, MJ/kg, derived from formation data rather than
// tabulated. TRANSCRIPTION of the same formation table, so it checks the
// derivation route and not the underlying enthalpies
inline constexpr double kH2LowerHeatingValue_MJ_kg = 119.96;

}  // namespace refdata
