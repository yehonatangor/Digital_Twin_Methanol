// Vertical vapour-liquid separator sizing against Turton's own worked examples
//
// Turton 5e Sec. 23.2.2.2, Eq. (23.10), Examples 23.2 and 23.3. Both examples'
// inputs are printed in the book; both printed diameters are reproduced here

#include "economics/vessel_sizing.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== vessel sizing (Turton Eq. 23.10) ===\n\n");

  // Example 23.2: water drops from air before a blower, 2 bar, 50 C
  std::printf("[1] Example 23.2 -- air/water, 2 bar\n");
  const double D1 = economics::vertical_vessel_diameter_m(3.865, 19.62e-6, 1000.0, 2.156);
  check("diameter", D1, 4.218, 0.002);

  // Example 23.3: benzene/toluene from fuel gas, 24 bar
  std::printf("\n[2] Example 23.3 -- benzene/toluene from fuel gas, 24 bar\n");
  const double D2 = economics::vertical_vessel_diameter_m(0.6967, 5.05e-6, 848.5, 7.8839);
  check("diameter", D2, 0.990, 0.002);

  // Table 23.10 L/D bands, midpoints
  std::printf("\n[3] Table 23.10 L/D ratios (midpoints of the stated bands)\n");
  check("below 18 bar",  economics::vessel_L_over_D_ratio(10.0), 2.25, 1e-12);
  check("18 to 36 bar",  economics::vessel_L_over_D_ratio(24.0), 3.5,  1e-12);
  check("above 36 bar",  economics::vessel_L_over_D_ratio(78.0), 5.0,  1e-12);

  std::printf("\n[4] Guards\n");
  checkTrue("rho_l <= rho_g is rejected",
            economics::vertical_vessel_diameter_m(1.0, 1e-5, 2.0, 5.0) < 0.0);
  checkTrue("non-positive flow is rejected",
            economics::vertical_vessel_diameter_m(0.0, 1e-5, 1000.0, 2.0) < 0.0);

  std::printf("\n[5] Full sizing wraps diameter, length and volume consistently\n");
  const auto s = economics::size_vertical_vessel(3.865, 19.62e-6, 1000.0, 2.156, 2.0);
  checkTrue("ok", s.ok);
  check("diameter", s.diameter_m, 4.218, 0.002);
  check("length = D * 2.25 below 18 bar", s.length_m, 4.218 * 2.25, 0.01);
  checkRel("volume = pi/4 D^2 L", s.volume_m3,
           0.25 * 3.14159265358979323846 * s.diameter_m * s.diameter_m * s.length_m, 1e-12);

  return report("vessel sizing");
}
