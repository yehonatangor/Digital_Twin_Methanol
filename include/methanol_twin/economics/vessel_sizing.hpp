#pragma once
// Vertical vapour-liquid separator sizing
//
// Turton 5th ed. Sec. 23.2.2.2 Eq. (23.10), diameter for a 100 um design
// droplet:  D = 15300 * sqrt(Qg * mu_g / (rho_l - rho_g)), D in m
// The 15300 folds in Stokes' law and Ug = Qg/(pi D^2/4); reproduces the book's
// Examples 23.2 and 23.3 exactly. Equation OCR'd from a page render, the PDF
// text layer does not carry it
//
// Length from Table 23.10's L/D ratios, midpoint of each stated range
// Vertical only; horizontal (Eq. 23.11-23.13) is not implemented
// mu_g comes from reactor/transport.hpp and carries that module's own flag
// See docs/20-equipment-costing.md

#include <string>

namespace economics {


// Eq. (23.10). Negative if rho_l <= rho_g
double vertical_vessel_diameter_m(double Qg_m3_s, double mu_g_Pa_s, double rho_l_kg_m3, double rho_g_kg_m3);

// Table 23.10 midpoint, by design pressure
double vessel_L_over_D_ratio(double design_pressure_bar);

struct VesselSizingResult {
  double diameter_m = 0.0;
  double length_m = 0.0;
  double volume_m3 = 0.0;   // pi/4 D^2 L, the size argument for Table A.1
  bool ok = false;
  std::string message;
};

// Diameter from Eq. (23.10), length from Table 23.10
VesselSizingResult size_vertical_vessel(double Qg_m3_s, double mu_g_Pa_s, double rho_l_kg_m3,
                                          double rho_g_kg_m3, double design_pressure_bar);

}  // namespace economics
