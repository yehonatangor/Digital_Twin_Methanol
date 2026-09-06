#pragma once
// Fixed-bed pressure gradient. Van-Dal & Bouallou (2013) Sec. 2.3.2 states the
// Ergun equation is used but reports no dP value, so there is no exact gate

#include "transport.hpp"

namespace reactor {

// Lengths m, densities kg/m3. particle_density_kg_m3 is the particle density,
// not the bulk bed density
struct BedGeometry {
  double tube_inner_diameter_m  = 0.0;
  double bed_length_m           = 0.0;
  int    n_tubes                = 1;
  double void_fraction          = 0.0;   // eps
  double particle_diameter_m    = 0.0;   // dp
  double particle_density_kg_m3 = 0.0;   // rho_p
  const char* source            = "unset";
};

double cross_section_area_m2(const BedGeometry& bed);   // per tube
double bulk_density_kg_m3(const BedGeometry& bed);      // rho_p * (1 - eps)
double catalyst_mass_per_tube_kg(const BedGeometry& bed);
double catalyst_mass_total_kg(const BedGeometry& bed);

enum class PressureDropModel { None, Ergun, ConstantTotal };

struct PressureDropConfig {
  PressureDropModel model = PressureDropModel::Ergun;
  double constant_total_drop_Pa = 1.0e5;   // Mucci (2023) App. A.4, 1 bar/stage
};

// Ergun (1952), mass-flux form (G = rho*u_s is constant down the bed):
//  -dP/dz = 150*mu*(1-e)^2*G/(rho*dp^2*e^3) + 1.75*(1-e)*G^2/(rho*dp*e^3)
// Returns a NEGATIVE number (pressure falls in the flow direction), Pa/m
double ergun_dPdz_Pa_per_m(double mu_Pa_s, double rho_kg_m3,
                           double G_kg_m2s, const BedGeometry& bed);

// Re_p = dp*G/mu. << 10 => viscous branch; >> 1000 => inertial
double particle_reynolds(double G_kg_m2s, double mu_Pa_s, const BedGeometry& bed);

// W = rho_bulk*A_c*z  =>  dP/dW = (dP/dz)/(rho_bulk*A_c). W and molar flows are
// per tube; multiply by n_tubes at the plant boundary
double dPdz_to_dPdW(double dPdz_Pa_per_m, const BedGeometry& bed);
double dPdW_to_dPdz(double dPdW_Pa_per_kg, const BedGeometry& bed);

double pressure_gradient_dPdW(const PressureDropConfig& cfg, double mu_Pa_s,
                              double rho_kg_m3, double G_kg_m2s,
                              const BedGeometry& bed);

namespace presets {
// Van-Dal Tables A.2/A.3 lab reactor. The stated set over-determines the bed
// (mass, eps and L are mutually inconsistent), so two presets expose the fork
BedGeometry van_dal_lab_mass_primary();     // mass + eps primary, L derived
BedGeometry van_dal_lab_length_primary();   // L as stated, mass implied ~26.8 g

// Van-Dal Table 2 catalyst in Shi et al. Sec. 3.1 tubes. Declared design basis
BedGeometry van_dal_industrial();

// Shi et al. (2020) Sec. 3.1, the boiling-water reactor
BedGeometry shi_bwr();
}  // namespace presets

void report_pressure_drop(const BedGeometry& bed, double mu_Pa_s,
                          double rho_kg_m3, double G_kg_m2s, double P_inlet_Pa);

}  // namespace reactor

