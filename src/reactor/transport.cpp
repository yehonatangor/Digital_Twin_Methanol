#include "transport.hpp"
#include <cmath>
#include <cstdio>

namespace reactor {

double molarMassKgPerMol(Species sp) {
  return properties(sp).molar_mass / 1000.0;   // species.hpp stores g/mol
}

// DIPPR-102 vapour viscosity, one row per Species in enum order:
//   mu(T) = A * T^B / (1 + C/T + D/T^2)   [Pa.s], T in K
// Layout: {A, B, C, D, Tmin, Tmax, sourced, "source"}
//
// PROXIMATE source, what was actually read: `chemicals` Python package (Bell
// et al., MIT), table mu_data_Perrys_8E_2_312. UNDERLYING source, what that
// digitises: Perry's 8e Table 2-312. Perry's was not opened directly, so the
// chain is stated rather than cited as primary. None of the four project
// papers report a gas viscosity or viscosity method
//
// Every row was cross-checked against an independent CRC/NIST value before
// being written here; worst deviation is H2O at +2.2 %, whose Perry's row is a
// bare two-parameter power law. See docs/10-transport-properties.md
//
// LOW-PRESSURE correlations. No Lucas/Chung high-pressure correction; valid
// while reduced density stays low, which it does in this loop
const std::array<Dippr102, static_cast<std::size_t>(NS)> kVaporViscosity{{
  {2.1480e-06, 0.46000, 290.0000,   0.0, 194.67, 1500.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):CO2"},
  {1.7970e-07, 0.68500,  -0.5900, 140.0,  13.95, 3000.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):H2"},
  {1.1127e-06, 0.53380,  94.7000,   0.0,  68.15, 1250.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):CO"},
  {1.7096e-08, 1.11460,   0.0000,   0.0, 273.16, 1073.15, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):H2O"},
  {3.0663e-07, 0.69655, 205.0000,   0.0, 240.00, 1000.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):CH3OH"},
  {5.2546e-07, 0.59006, 105.6700,   0.0,  90.69, 1000.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):CH4"},
  {6.5592e-07, 0.60810,  54.7140,   0.0,  63.15, 1970.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):N2"},
  {9.2121e-07, 0.60529,  83.2400,   0.0,  83.78, 3273.10, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):Ar"},
  {1.1010e-06, 0.56340,  96.3000,   0.0,  54.35, 1500.00, true,
   "chemicals pkg / Perry's 8e Table 2-312 (DIPPR-102):O2"}
}};

double pure_viscosity_Pa_s(Species sp, double T_K) {
  const Dippr102& c = kVaporViscosity[idx(sp)];
  if (!c.sourced || T_K <= 0.0) return -1.0;
  const double denom = 1.0 + c.C / T_K + c.D / (T_K * T_K);
  if (std::fabs(denom) < 1e-300) return -1.0;
  return c.A * std::pow(T_K, c.B) / denom;
}

double mixture_viscosity_wilke_Pa_s(const SpeciesArray& y, double T_K) {
  std::array<double, static_cast<std::size_t>(NS)> mu{};
  for (int i = 0; i < NS; ++i) {
    const Species sp = static_cast<Species>(i);
    mu[idx(sp)] = pure_viscosity_Pa_s(sp, T_K);
    if (mu[idx(sp)] < 0.0 && y[idx(sp)] > 0.0) return -1.0;  // active + unsourced
  }
  double mu_mix = 0.0;
  for (int i = 0; i < NS; ++i) {
    const std::size_t si = static_cast<std::size_t>(i);
    if (y[si] <= 0.0) continue;
    const double Mi = molarMassKgPerMol(static_cast<Species>(i));
    double denom = 0.0;
    for (int j = 0; j < NS; ++j) {
      const std::size_t sj = static_cast<std::size_t>(j);
      if (y[sj] <= 0.0) continue;
      const double Mj = molarMassKgPerMol(static_cast<Species>(j));
      const double num = 1.0 + std::sqrt(mu[si] / mu[sj]) * std::pow(Mj / Mi, 0.25);
      denom += y[sj] * (num * num) / std::sqrt(8.0 * (1.0 + Mi / Mj));
    }
    if (denom > 0.0) mu_mix += y[si] * mu[si] / denom;
  }
  return mu_mix;
}

double mixture_viscosity_Pa_s(const SpeciesArray& y, double T_K,
                              const ViscosityConfig& cfg) {
  if (cfg.use_constant) return cfg.constant_mu_Pa_s;
  const double mu = mixture_viscosity_wilke_Pa_s(y, T_K);
  return (mu <= 0.0) ? -1.0 : mu;
}

bool viscosity_data_complete() {
  for (const auto& c : kVaporViscosity) if (!c.sourced) return false;
  return true;
}

void check_viscosity_sources() {
  std::printf("--- Vapour viscosity source check (DIPPR-102) ---\n");
  for (int i = 0; i < NS; ++i) {
    const Species sp = static_cast<Species>(i);
    const Dippr102& c = kVaporViscosity[idx(sp)];
    std::printf("  [%s] %-6s : %s\n", c.sourced ? "OK  " : "SKIP",
                speciesName(sp), c.source);
  }
  std::printf("--- complete: %s ---\n",
              viscosity_data_complete() ? "yes" : "NO (constant fallback in use)");
}

}  // namespace reactor

