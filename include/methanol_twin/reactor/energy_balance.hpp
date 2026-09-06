#pragma once
#include <array>
#include "ergun.hpp"
#include "species.hpp"
#include "thermo.hpp"
#include "transport.hpp"

namespace reactor {

inline constexpr int N_RXN = 2;

enum Rxn : int {
  R_MEOH = 0,
  R_RWGS = 1
};

// Rows: methanol synthesis, RWGS. Columns follow the Species enum order
inline constexpr std::array<std::array<double, static_cast<std::size_t>(NS)>, static_cast<std::size_t>(N_RXN)> kStoich{{
  {  -1.0, -3.0,  0.0,  1.0,  1.0,   0.0,  0.0,  0.0,  0.0 },
  {  -1.0, -1.0,  1.0,  1.0,  0.0,   0.0,  0.0,  0.0,  0.0 }
}};

enum class ThermalMode { Adiabatic, Cooled, Isothermal };

struct CoolingConfig {
  double coolant_T_K = 518.15; // 245 C, Mucci (2023) App. A.4


  // OVERALL DERIVED HEAT-TRANSFER COEFFICIENT
  
  // Full working: docs/U_Derivation.md. 
  // Summary of how this number was reached, the derivation matters more than the value:
  
  //   Shi et al. (2020) Sec. 3.1 publishes the BWR geometry and duty exactly:
  //     A = 2700 tubes x pi x 0.035 m x 7.0 m       = 2078.2 m^2
  //     Q = 61 MW absorbed to raise high-pressure steam
  //     gas 250 C in (stream 22) -> 275 C out (stream 23)
  
  //   Cross-checked independently: 2095 t/day of methanol at -91 kJ/mol for CO hydrogenation implies 68.9 MW 
  //   before the endothermic RWGS contribution, so Shi's 61 MW is internally consistent with his own production rate
  
  //   Required flux:  Q/A = 61e6 / 2078.2 = 29,352 W/m^2
  //   With the coolant at 245 C and the bed sitting near its 275 C outlet over most of its length once past the inlet rise
  //   The effective driving force is ~30 K, giving U = 29,352 / 30 ~= 980 W/(m^2 K)
  
  // Shi never states the shell-side steam pressure, so the driving force is not published and U is under-determined by his data
  // It spans 359 to 2104 W/(m^2 K) across plausible steam pressures. The 245 C coolant is imported from Mucci, not from Shi 
  // U is DERIVED from published quantities under a stated assumption, and U_sourced stays false
  
  // UNCERTAINTY BAND, for any result that depends on this: 700-1400 W/(m^2 K),
  // corresponding to an effective driving force of 42 K (a 287 C hot spot) down to 21 K (inlet-weighted)
  // Carry this band into sensitivity analysis 
  // see docs/U_Derivation.md Sec. 7

  double U_W_m2K = 980.0;
  bool   U_sourced = false; // DERIVED under a stated assumption
  const char* U_source =
      "DERIVED from Shi et al. (2020) Sec. 3.1 (A = 2078.2 m^2, Q = 61 MW, "
      "275 C outlet) with Mucci's 245 C coolant; band 700-1400. "
      "See docs/U_Derivation.md";
};

double delta_H_rxn_J_per_mol(int j, double T_K);

double stream_heat_capacity_W_per_K(const SpeciesArray& F, double T_K);

double specific_transfer_area_m2_per_kg(const BedGeometry& bed);

double heat_removal_W_per_kg(double T_K, const CoolingConfig& cool, const BedGeometry& bed);

double dTdW_K_per_kg(const SpeciesArray& F, double T_K, const std::array<double, static_cast<std::size_t>(N_RXN)>& rates,
                     ThermalMode mode, const CoolingConfig& cool, const BedGeometry& bed);

void audit_reaction_enthalpies();

}

