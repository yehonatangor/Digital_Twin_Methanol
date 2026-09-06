#include "vessel_sizing.hpp"

#include <cmath>

namespace economics {

double vertical_vessel_diameter_m(double Qg_m3_s, double mu_g_Pa_s, double rho_l_kg_m3,
                                   double rho_g_kg_m3) {
  const double drho = rho_l_kg_m3 - rho_g_kg_m3;
  if (!(drho > 0.0)) return -1.0;   // liquid must be the denser phase
  if (!(Qg_m3_s > 0.0) || !(mu_g_Pa_s > 0.0)) return -1.0;
  // Turton Eq. (23.10)
  return 15300.0 * std::sqrt(Qg_m3_s * mu_g_Pa_s / drho);
}

double vessel_L_over_D_ratio(double design_pressure_bar) {
  // Table 23.10, midpoint of each stated range
  if (design_pressure_bar < 18.0) return 2.25;   // 2.0-2.5
  if (design_pressure_bar <= 36.0) return 3.5;   // 3.0-4.0
  return 5.0;                                     // 4.0-6.0
}

VesselSizingResult size_vertical_vessel(double Qg_m3_s, double mu_g_Pa_s, double rho_l_kg_m3,
                                          double rho_g_kg_m3, double design_pressure_bar) {
  VesselSizingResult r;

  const double D = vertical_vessel_diameter_m(Qg_m3_s, mu_g_Pa_s, rho_l_kg_m3, rho_g_kg_m3);
  if (!(D > 0.0)) {
    r.message = "Eq. (23.10) inputs invalid: need Qg > 0, mu_g > 0 and rho_l > rho_g";
    return r;
  }

  r.diameter_m = D;
  r.length_m   = D * vessel_L_over_D_ratio(design_pressure_bar);
  r.volume_m3  = 0.25 * 3.14159265358979323846 * D * D * r.length_m;
  r.ok = true;
  r.message = "ok";
  return r;
}

}  // namespace economics
