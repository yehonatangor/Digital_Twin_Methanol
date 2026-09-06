#include "ergun.hpp"
#include <cmath>
#include <cstdio>

namespace reactor {

namespace { constexpr double kPi = 3.14159265358979323846; }

double cross_section_area_m2(const BedGeometry& bed) {
  const double d = bed.tube_inner_diameter_m;
  return 0.25 * kPi * d * d;
}

double bulk_density_kg_m3(const BedGeometry& bed) {
  // rho_p is per m3 of catalyst, so the bed average is rho_p*(1-eps)
  return bed.particle_density_kg_m3 * (1.0 - bed.void_fraction);
}

double catalyst_mass_per_tube_kg(const BedGeometry& bed) {
  return bulk_density_kg_m3(bed) * cross_section_area_m2(bed) * bed.bed_length_m;
}

double catalyst_mass_total_kg(const BedGeometry& bed) {
  return catalyst_mass_per_tube_kg(bed) * static_cast<double>(bed.n_tubes);
}

double ergun_dPdz_Pa_per_m(double mu_Pa_s, double rho_kg_m3,
                           double G_kg_m2s, const BedGeometry& bed) {
  if (mu_Pa_s <= 0.0 || rho_kg_m3 <= 0.0) return 0.0;
  const double eps = bed.void_fraction, dp = bed.particle_diameter_m;
  const double om = 1.0 - eps, eps3 = eps * eps * eps;
  const double viscous  = 150.0 * mu_Pa_s * om * om * G_kg_m2s
                        / (rho_kg_m3 * dp * dp * eps3);
  const double inertial = 1.75 * om * G_kg_m2s * G_kg_m2s / (rho_kg_m3 * dp * eps3);
  return -(viscous + inertial);
}

double particle_reynolds(double G_kg_m2s, double mu_Pa_s, const BedGeometry& bed) {
  if (mu_Pa_s <= 0.0) return 0.0;
  return bed.particle_diameter_m * G_kg_m2s / mu_Pa_s;
}

double dPdz_to_dPdW(double dPdz_Pa_per_m, const BedGeometry& bed) {
  const double denom = bulk_density_kg_m3(bed) * cross_section_area_m2(bed);
  return (denom <= 0.0) ? 0.0 : dPdz_Pa_per_m / denom;
}

double dPdW_to_dPdz(double dPdW_Pa_per_kg, const BedGeometry& bed) {
  return dPdW_Pa_per_kg * bulk_density_kg_m3(bed) * cross_section_area_m2(bed);
}

double pressure_gradient_dPdW(const PressureDropConfig& cfg, double mu_Pa_s,
                              double rho_kg_m3, double G_kg_m2s,
                              const BedGeometry& bed) {
  switch (cfg.model) {
    case PressureDropModel::None: return 0.0;
    case PressureDropModel::ConstantTotal: {
      const double W = catalyst_mass_per_tube_kg(bed);
      return (W <= 0.0) ? 0.0 : -cfg.constant_total_drop_Pa / W;
    }
    case PressureDropModel::Ergun:
    default:
      return dPdz_to_dPdW(ergun_dPdz_Pa_per_m(mu_Pa_s, rho_kg_m3, G_kg_m2s, bed), bed);
  }
}

namespace presets {

BedGeometry van_dal_lab_mass_primary() {
  BedGeometry b{};
  b.tube_inner_diameter_m  = 0.016;    // SOURCED: Van-Dal Appendix A text
  b.n_tubes                = 1;        // SOURCED: single tube, adiabatic
  b.void_fraction          = 0.5;      // SOURCED: Van-Dal Table A.2
  b.particle_diameter_m    = 0.0005;   // SOURCED: Van-Dal Table A.2
  b.particle_density_kg_m3 = 1775.0;   // SOURCED: Van-Dal Table A.2
  const double mass_kg  = 0.0348;      // SOURCED: Van-Dal Table A.2
  const double rho_bulk = 1775.0 * 0.5;
  const double area     = 0.25 * kPi * 0.016 * 0.016;
  b.bed_length_m = mass_kg / (rho_bulk * area);           // ~0.195 m
  b.source = "Van-Dal Tables A.2/A.3; L derived from mass + porosity "
             "(stated L = 0.15 m is inconsistent with stated mass)";
  return b;
}

BedGeometry van_dal_lab_length_primary() {
  BedGeometry b = van_dal_lab_mass_primary();
  b.bed_length_m = 0.15;   // SOURCED: Van-Dal Appendix A text
  b.source = "Van-Dal Tables A.2/A.3; L = 0.15 m as stated, implied catalyst "
             "mass ~26.8 g vs stated 34.8 g";
  return b;
}

// DECLARED DESIGN BASIS. Van-Dal publishes the industrial CATALYST (Table 2)
// but no tube count, diameter or bed length anywhere. The tube geometry is
// therefore Shi et al. (2020) Sec. 3.1, 0.035 m ID and 7.0 m length, for the
// same class of boiling-water-cooled multitubular converter
//
// Catalyst properties are Van-Dal's, tube geometry is Shi's, and the
// COMBINATION is this project's declared basis, not something either paper
// asserts. Any result depending on bed sizing inherits that
// flowsheet::presets::van_dal_catalyst_shi_tubes() is the composite actually
// used and derives the tube count from Van-Dal's own 44,500 kg charge
BedGeometry van_dal_industrial() {
  BedGeometry b{};
  b.void_fraction          = 0.4;      // SOURCED: Van-Dal Table 2
  b.particle_diameter_m    = 0.0055;   // SOURCED: Van-Dal Table 2 (5.5 mm)
  b.particle_density_kg_m3 = 1775.0;   // SOURCED: Van-Dal Table 2
  b.tube_inner_diameter_m  = 0.035;    // DECLARED BASIS: Shi et al. (2020) Sec. 3.1
  b.bed_length_m           = 7.0;      // DECLARED BASIS: Shi et al. (2020) Sec. 3.1
  b.n_tubes                = 2700;     // DECLARED BASIS: Shi et al. (2020) Sec. 3.1
  b.source = "Van-Dal Table 2 catalyst properties + Shi et al. (2020) Sec. 3.1 "
             "tube geometry -- DECLARED design basis, see ergun.cpp comment";
  return b;
}

BedGeometry shi_bwr() {
  BedGeometry b{};
  b.tube_inner_diameter_m  = 0.035;    // SOURCED: Shi et al. (2020) Sec. 3.1
  b.bed_length_m           = 7.0;      // SOURCED: Shi et al. (2020) Sec. 3.1
  b.n_tubes                = 2700;     // SOURCED: Shi et al. (2020) Sec. 3.1
  b.particle_diameter_m    = 0.005;    // SOURCED: Shi, spherical pellets
  b.void_fraction          = 0.40;     // SOURCED (derived): pellets fill 60%
  b.particle_density_kg_m3 = 1900.0;   // SOURCED (derived): 1140 bulk / 0.60
  b.source = "Shi et al. (2020) Sec. 3.1 (BWR geometry, pellet data)";
  return b;
}

}  // namespace presets

void report_pressure_drop(const BedGeometry& bed, double mu_Pa_s,
                          double rho_kg_m3, double G_kg_m2s, double P_inlet_Pa) {
  const double eps = bed.void_fraction, dp = bed.particle_diameter_m;
  const double om = 1.0 - eps, eps3 = eps * eps * eps;
  const double viscous  = 150.0 * mu_Pa_s * om * om * G_kg_m2s
                        / (rho_kg_m3 * dp * dp * eps3);
  const double inertial = 1.75 * om * G_kg_m2s * G_kg_m2s / (rho_kg_m3 * dp * eps3);
  const double total_dP = (viscous + inertial) * bed.bed_length_m;

  std::printf("--- Ergun diagnostic ---\n");
  std::printf("  bed source        : %s\n", bed.source);
  std::printf("  Re_p              : %.3g\n", particle_reynolds(G_kg_m2s, mu_Pa_s, bed));
  std::printf("  viscous  term     : %.4g Pa/m\n", viscous);
  std::printf("  inertial term     : %.4g Pa/m\n", inertial);
  std::printf("  total dP          : %.4g Pa  (%.4g bar)\n", total_dP, total_dP / 1.0e5);
  std::printf("  dP / P_inlet      : %.3g\n",
              P_inlet_Pa > 0.0 ? total_dP / P_inlet_Pa : 0.0);
  std::printf("------------------------\n");
}

}  // namespace reactor

