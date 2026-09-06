#pragma once
// Gas-phase viscosity for the Ergun pressure-drop term. DIPPR-102 coefficients from Perry's 8th ed. Table 2-312
// Wilke mixing rule from Poling et al. 5th ed. Eq. (9-5.13)

#include <array>
#include "species.hpp"

namespace reactor {

inline constexpr int NS = static_cast<int>(Species::Count);   // 9
using SpeciesArray = std::array<double, static_cast<std::size_t>(NS)>;

inline constexpr std::size_t idx(Species sp) { return static_cast<std::size_t>(sp); }

// Molar mass in kg/mol, from species.hpp (which stores g/mol)
double molarMassKgPerMol(Species sp);

// DIPPR-102 low-pressure vapour viscosity:
// mu(T) = A * T^B / (1 + C/T + D/T^2)      
// [Pa.s], T in [K]
struct Dippr102 {
  double A, B, C, D;
  double T_min_K, T_max_K;
  bool sourced; // false => placeholder, output must not be trusted
  const char* source;
};

extern const std::array<Dippr102, static_cast<std::size_t>(NS)> kVaporViscosity;

struct ViscosityConfig {
  bool use_constant = false;
  // Deliberate override only, not a fallback. Order-of-magnitude value for an H2-rich mixture near 500 K, not sourced
  double constant_mu_Pa_s = 1.8e-5;
};

// Negative sentinel (-1) when the component's coefficients are unsourced
double pure_viscosity_Pa_s(Species sp, double T_K);

// Wilke (1950) mixing rule, Poling et al. 5th ed. Eq. (9-5.13)
double mixture_viscosity_wilke_Pa_s(const SpeciesArray& y, double T_K);

// Negative on failure
double mixture_viscosity_Pa_s(const SpeciesArray& y, double T_K,
                              const ViscosityConfig& cfg);

bool viscosity_data_complete();
void audit_viscosity_sources();

}

