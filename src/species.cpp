#include "species.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace {

// {molar_mass [g/mol], Tc [K], Pc [Pa], omega [-]}. CRC Handbook / NIST,
// cross-checked against the `chemicals` databank. Methanol Tc and Pc are the
// IUPAC recommended values (512.50 K, 8.084 MPa); see docs/03.
const std::array<Properties, static_cast<size_t>(Species::Count)> table = {{
  {44.010, 304.21,  7.383e6,  0.224}, /*CO2*/
  { 2.016,  33.19,  1.313e6, -0.216}, /*H2*/
  {28.010, 132.92,  3.499e6,  0.048}, /*CO*/
  {18.015, 647.10, 22.064e6,  0.345}, /*H2O*/
  {32.042, 512.50,  8.084e6,  0.566}, /*CH3OH*/
  {16.043, 190.56,  4.599e6,  0.012}, /*CH4*/
  {28.013, 126.20,  3.400e6,  0.038}, /*N2*/
  {39.948, 150.86,  4.898e6, -0.004}, /*Ar*/
  {31.999, 154.58,  5.043e6,  0.022}, /*O2*/
}};

// DIPPR-107 (eq 2) and DIPPR-100 (eq 1) ideal-gas Cp, J/(kmol K).
// Perry's Chemical Engineers' Handbook 8th ed., Table 2-155.
const std::array<CpParams, static_cast<size_t>(Species::Count)> cp_table = {{
  {2, 29370.00, 34540.00, 1428.00, 26400.00,  588.00, 0.00, 0.00 }, /*CO2*/
  {2, 27617.00,  9560.00, 2466.00,  3760.00,  567.60, 0.00, 0.00 }, /*H2*/
  {2, 29108.00,  8773.00, 3085.10,  8455.30, 1538.20, 0.00, 0.00 }, /*CO*/
  {2, 33363.00, 26790.00, 2610.50,  8896.00, 1169.00, 0.00, 0.00 }, /*H2O*/
  {3, 33258.00, 36199.00, 1205.70, 15373000.00, 3212.20, -1.5318e7, 3212.20 }, /*CH3OH*/
  {2, 33298.00, 79933.00, 2086.90, 41602.00,  991.96, 0.00, 0.00 }, /*CH4*/
  {2, 29105.00,  8614.90, 1701.60,   103.47,  909.79, 0.00, 0.00 }, /*N2*/
  {1, 20786.00,     0.00,    0.00,     0.00,    0.00, 0.00, 0.00 }, /*Ar*/
  {2, 29103.00, 10040.00, 2526.50,  9356.00, 1153.80, 0.00, 0.00 }, /*O2*/
}};

// DIPPR-101 vapour pressure, Pa. Perry's 8th ed., Table 2-8.
const std::unordered_map<Species, VaporPressureParams> vp_table = {
  { Species::H2O,   {7.3649e01, -7.2582e03, -7.3037e00, 4.1653e-06, 2.00} },
  { Species::CH3OH, {8.2718e01, -6.9045e03, -8.8622e00, 7.4664e-06, 2.00} },
  { Species::CO2,   {4.70169e01, -2.8390e03, -3.86388e00, 2.81e-16, 6.00} },
};

}

const Properties& properties(Species sp) {
  auto idx = static_cast<size_t>(sp);
  if (idx >= table.size()) throw std::out_of_range("properties: invalid species");
  return table[idx];
}

const CpParams& cpParams(Species sp) {
  auto idx = static_cast<size_t>(sp);
  if (idx >= cp_table.size()) throw std::out_of_range("cpParams: invalid species");
  return cp_table[idx];
}

const VaporPressureParams* vaporPressureParams(Species sp) {
  auto it = vp_table.find(sp);
  return (it != vp_table.end()) ? &it->second : nullptr;
}

double vaporPressure(Species sp, double T) {
  const VaporPressureParams* p = vaporPressureParams(sp);
  if (!p) {
    return 1.0e10;   // no data: treat as permanently non-condensable
  }
  if (p->A == 0.0 && p->B == 0.0) {
    throw std::runtime_error("vaporPressure: empty vp_table entry");
  }

  return std::exp(p->A + p->B / T + p->C * std::log(T) + p->D * std::pow(T, p->E));
}

const char* speciesName(Species sp) {
  switch (sp) {
    case Species::CO2: return "CO2";
    case Species::H2: return "H2";
    case Species::CO: return "CO";
    case Species::H2O: return "H2O";
    case Species::CH3OH: return "CH3OH";
    case Species::CH4: return "CH4";
    case Species::N2: return "N2";
    case Species::Ar: return "Ar";
    case Species::O2: return "O2";
    default: return "?";
  }
}

