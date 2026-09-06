#pragma once
// Distillation column diameter, Fair/Matthews flooding correlation
//
// Turton 5th ed. Sec. 21.3.2.2, constants curve-fit by Wankat, Table 21.7
// Equations OCR'd from page renders; reproduce Example 21.4 exactly
//   (21.59)  Flv   = (L*ML)/(V*MV) * sqrt(rhoV/rhoL)          [dimensionless]
//   (21.58)  log10(Csb,flood) = -a - b*log10(Flv) - c*log10(Flv)^2
//   (21.60)  Csb,flood = uf*(sigma/20)^0.2 * sqrt(rhoV/(rhoL-rhoV))
//            uf in ft/s, sigma in dyne/cm. Turton states no metric form
//   (21.61)  active area = (V*MV)/(rhoV*u_actual), u_actual = f_flood*uf
//            actual area = active/f_active,  D = sqrt(4*A/pi)
//
// sigma defaults to 20 dyne/cm, making that term exactly 1, which is what
// Turton's own Example 21.4 does and states. f_flood 0.75 and f_active 0.88
// are the example's values, inside Turton's stated typical ranges
//
// Sized at both top and bottom, larger diameter taken
// front_end/distillation.hpp is a lumped recovery model with no internal
// reflux or stage count, so the real column conditions this needs are built
// here from a bubble point rather than inherited from it
// See docs/20-equipment-costing.md

#include <string>

#include "front_end/distillation.hpp"
#include "stream.hpp"

namespace economics {

// ---- Part 1: pure Turton equations (Eq. 21.58-21.61), no dependency on
// this project's Stream/thermo machinery -- plain physical inputs only. ----

struct TraySpacingConstants {
  double a, b, c;
  double tray_spacing_in;
};
namespace presets {
TraySpacingConstants fair_wankat_6in();
TraySpacingConstants fair_wankat_9in();
TraySpacingConstants fair_wankat_12in();
TraySpacingConstants fair_wankat_18in();
TraySpacingConstants fair_wankat_24in();  // Turton's own Example 21.4 uses this
TraySpacingConstants fair_wankat_36in();
}  // namespace presets

// Eq. (21.59). ML_kg_per_mol/MV_kg_per_mol default handling (ML~=MV) is the
// CALLER's choice -- pass the same value for both to reproduce Turton's own
// worked-example simplification exactly
double flooding_flow_parameter(double L_mol_s, double V_mol_s, double ML_kg_per_mol, double MV_kg_per_mol,
                                double rho_V_kg_m3, double rho_L_kg_m3);

// Eq. (21.58)
double flooding_capacity_factor(double Flv, const TraySpacingConstants& cfg);

// Eq. (21.60), solved for uf, returned in m/s (converted from the
// equation's native ft/s -- see header). sigma_dyne_per_cm=20.0 makes the
// surface-tension correction term exactly 1.0, per Turton's own stated
// approximation (see header)
double flooding_velocity_m_s(double Csb_flood, double rho_L_kg_m3, double rho_V_kg_m3,
                              double sigma_dyne_per_cm = 20.0);

struct ColumnDiameterResult {
  double Flv = 0.0;
  double Csb_flood = 0.0;
  double uf_m_s = 0.0;
  double u_actual_m_s = 0.0;
  double active_area_m2 = 0.0;
  double actual_area_m2 = 0.0;
  double diameter_m = 0.0;
  bool ok = false;
  std::string message;
};
// fraction_flooding, active_area_fraction default to Turton's own Example
// 21.4 values (0.75, 0.88) -- see header
ColumnDiameterResult size_column_diameter(double L_mol_s, double V_mol_s, double ML_kg_per_mol,
                                            double MV_kg_per_mol, double rho_L_kg_m3, double rho_V_kg_m3,
                                            const TraySpacingConstants& tray_spacing_cfg,
                                            double fraction_flooding = 0.75, double active_area_fraction = 0.88,
                                            double sigma_dyne_per_cm = 20.0);

// ---- Part 2: bubble point, using this project's own NRTL + DIPPR machinery ----

struct BubblePointResult {
  double T_K = 0.0;
  int    n_iterations = 0;
  bool   converged = false;
  bool   ok = false;
  std::string message;
};
// Solves sum_i( x_i * gamma_i(x, T) * Psat_i(T) ) = P for T. Uses only the
// mole fractions of `liquid`, not its own T or P. Newton with a bisection
// fallback, no external root-finding library
BubblePointResult bubble_point_T_K(const Stream& liquid, double P_Pa, double T_guess_K = 350.0);

// ---- Part 3: the full wrapper -- crude-feed-in, column-diameter-out ----

struct DistillationColumnSizingConfig {
  // SOURCED: Van-Dal & Bouallou (2013), "44 rectifying and 13 stripping
  // stages at a 1.2 reflux ratio" (already cited, Chapter 6.9's header)
  double reflux_ratio = 1.2;
  // SOURCED: same citation, 44+13 total equilibrium stages
  int    total_stages = 57;

  // SOURCED: Van-Dal Sec. 2.3.3, "The crude methanol is expanded to 1.2 bar in
  // two [stages]" before the column. That is the stream pressure entering the
  // column; using it as the whole column's pressure assumes negligible tray
  // drop, which is this project's modelling step, not Van-Dal's claim
  double column_pressure_bar = 1.2;
  bool   column_pressure_sourced = true;

  // Physically motivated (feed IS knockout-drum condensate) -- see header
  bool   feed_is_saturated_liquid = true;

  TraySpacingConstants tray_spacing_cfg = presets::fair_wankat_24in();
  double fraction_flooding = 0.75;
  double active_area_fraction = 0.88;
  double sigma_dyne_per_cm = 20.0;
  double T_guess_K = 350.0;
};

struct DistillationColumnSizingResult {
  double D_mol_s = 0.0, B_mol_s = 0.0, F_mol_s = 0.0;
  double L_top_mol_s = 0.0, V_top_mol_s = 0.0, L_bottom_mol_s = 0.0, V_bottom_mol_s = 0.0;

  BubblePointResult top_bubble_point;      // distillate composition, at column_pressure_bar
  BubblePointResult bottom_bubble_point;   // bottoms composition, at column_pressure_bar

  double rho_V_top_kg_m3 = 0.0, rho_L_top_kg_m3 = 0.0;
  double rho_V_bottom_kg_m3 = 0.0, rho_L_bottom_kg_m3 = 0.0;

  ColumnDiameterResult top_section;
  ColumnDiameterResult bottom_section;
  double diameter_m = 0.0;   // max(top, bottom) -- Turton's own stated practice: size for the limiting section

  bool ok = false;
  std::string message;
};

// `crude_feed` is the feed distillation.hpp actually purified (i.e. the
// SAME stream passed to front_end::purify_methanol()); `distillation` is
// that call's own result
DistillationColumnSizingResult size_distillation_column(
    const Stream& crude_feed, const front_end::DistillationResult& distillation,
    const DistillationColumnSizingConfig& cfg = DistillationColumnSizingConfig{});

}  // namespace economics
