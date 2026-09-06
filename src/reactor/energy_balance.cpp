#include "energy_balance.hpp"
#include <cmath>
#include <cstdio>

namespace reactor {

namespace { 
  constexpr double kT_ref_K = 298.15; 
}

double delta_H_rxn_J_per_mol(int j, double T_K) {
  if (j < 0 || j >= N_RXN) return 0.00;
  double s = 0.00;
  for (int i = 0; i < NS; ++i) {
    const double nu = kStoich[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
    if (nu != 0.00) s += nu * thermo::enthalpy(static_cast<Species>(i), T_K);
  }
  return s;
}

double stream_heat_capacity_W_per_K(const SpeciesArray& F, double T_K) {
  double s = 0.00;
  for (int i = 0; i < NS; ++i) {
    const double Fi = F[static_cast<std::size_t>(i)];
    if (Fi > 0.00) s += Fi * thermo::cp(static_cast<Species>(i), T_K);
  }
  return s;
}

double specific_transfer_area_m2_per_kg(const BedGeometry& bed) {
  const double rho_bulk = bulk_density_kg_m3(bed);
  if (rho_bulk <= 0.00 || bed.tube_inner_diameter_m <= 0.00) return 0.00;
  return 4.00 / (rho_bulk * bed.tube_inner_diameter_m);
}

double heat_removal_W_per_kg(double T_K, const CoolingConfig& cool, const BedGeometry& bed) {
  return cool.U_W_m2K * specific_transfer_area_m2_per_kg(bed) * (T_K - cool.coolant_T_K);
}

double dTdW_K_per_kg(const SpeciesArray& F, double T_K, const std::array<double, static_cast<std::size_t>(N_RXN)>& rates,
                     ThermalMode mode, const CoolingConfig& cool, const BedGeometry& bed) {
  if (mode == ThermalMode::Isothermal) return 0.00;

  double q_gen = 0.00;
  for (int j = 0; j < N_RXN; ++j)
    q_gen += (-delta_H_rxn_J_per_mol(j, T_K)) * rates[static_cast<std::size_t>(j)];

  const double q_rem = (mode == ThermalMode::Cooled) ? heat_removal_W_per_kg(T_K, cool, bed) : 0.00;

  const double sum_FCp = stream_heat_capacity_W_per_K(F, T_K);
  if (sum_FCp <= 0.00) return 0.00;
  return (q_gen - q_rem) / sum_FCp;
}

void check_reaction_enthalpies() {
  std::printf("--- Reaction enthalpy check (from thermo.cpp formation data) ---\n");
  std::printf("  R_MEOH  dH(298) = %+8.2f kJ/mol   Shi Eq.(8) gas basis: -49\n", delta_H_rxn_J_per_mol(R_MEOH, kT_ref_K) / 1000.00);
  std::printf("  R_RWGS  dH(298) = %+8.2f kJ/mol   Van-Dal Eq.(3):      +41\n", delta_H_rxn_J_per_mol(R_RWGS, kT_ref_K) / 1000.00);
  std::printf("  (Van-Dal Eq.(2) -87 kJ/mol is the CH3OH(l) basis -- do not use)\n");
  std::printf("---------------------------------------------------------------\n");
}

}

