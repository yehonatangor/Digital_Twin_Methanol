// Distillation column diameter against Turton's own worked Example 21.4
//
// Turton 5e Sec. 21.3.2.2, Eq. (21.58)-(21.61), Table 21.7 (constants sourced
// there to Wankat, "Separation Processes," 4th ed.)
//
// Example 21.4 prints Csb,f = 0.3054 at 24-in tray spacing and a flooding
// velocity of 3.025 ft/s, with the surface-tension term taken as unity. The
// flow parameter that produces that Csb is 0.145

#include "economics/column_sizing.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== column sizing (Turton Eq. 21.58-21.61) ===\n\n");

  const auto t24 = economics::presets::fair_wankat_24in();

  std::printf("[1] Table 21.7 constants, 24-in row\n");
  check("a", t24.a, 0.94506, 1e-12);
  check("b", t24.b, 0.70234, 1e-12);
  check("c", t24.c, 0.22618, 1e-12);
  check("tray spacing", t24.tray_spacing_in, 24.0, 1e-12);

  std::printf("\n[2] Table 21.7, remaining rows\n");
  check("6 in,  a", economics::presets::fair_wankat_6in().a,  1.1977,  1e-12);
  check("9 in,  a", economics::presets::fair_wankat_9in().a,  1.1622,  1e-12);
  check("12 in, a", economics::presets::fair_wankat_12in().a, 1.0674,  1e-12);
  check("18 in, a", economics::presets::fair_wankat_18in().a, 1.0262,  1e-12);
  check("36 in, a", economics::presets::fair_wankat_36in().a, 0.85984, 1e-12);

  std::printf("\n[3] Example 21.4 -- Eq. (21.58) at Flv = 0.145\n");
  const double Csb = economics::flooding_capacity_factor(0.145, t24);
  check("Csb,flood", Csb, 0.3054, 0.0002);

  std::printf("\n[4] Example 21.4 -- Eq. (21.60) flooding velocity\n");
  // The book's own uf/Csb ratio implies (rho_l - rho_v)/rho_v = 98.1 at the
  // bottom of the column; rho_v = 6.1 kg/m^3 for hexane vapour at 2 atm
  const double rho_v = 6.1;
  const double rho_l = rho_v * (1.0 + (3.025 / 0.3054) * (3.025 / 0.3054));
  const double uf = economics::flooding_velocity_m_s(0.3054, rho_l, rho_v, 20.0);
  check("uf, ft/s", uf / 0.3048, 3.025, 0.005);
  check("u at 75 % of flooding, m/s", 0.75 * uf, 0.692, 0.002);

  std::printf("\n[5] sigma = 20 dyne/cm makes the surface-tension term exactly 1\n");
  checkRel("uf is unchanged at sigma = 20",
           economics::flooding_velocity_m_s(0.3054, rho_l, rho_v, 20.0), uf, 1e-12);
  checkTrue("a higher sigma lowers uf",
            economics::flooding_velocity_m_s(0.3054, rho_l, rho_v, 72.0) < uf);

  std::printf("\n[6] Eq. (21.59) flow parameter\n");
  // Equal molar masses cancel, which is the simplification Example 21.4 makes
  check("Flv = (L/V) sqrt(rhoV/rhoL)",
        economics::flooding_flow_parameter(1.0, 2.0, 0.086, 0.086, rho_v, rho_l),
        0.5 * std::sqrt(rho_v / rho_l), 1e-12);

  std::printf("\n[7] Guards\n");
  const auto bad = economics::size_column_diameter(1.0, 2.0, 0.086, 0.086, 5.0, 600.0, t24);
  checkTrue("rho_L < rho_V is rejected", !bad.ok);

  return report("column sizing");
}
