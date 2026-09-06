#pragma once
// Cu/ZnO/Al2O3 methanol catalyst activity decay

// Fichtl et al., Appl. Catal. A 502 (2015) 262-270, Table 4: power-law decay
// da/dt = -Kd(T) * a^m
// Kd = A exp(-Ea/RT)   
// Units [1/h]
// Order m and Kd fitted per catalyst at 493, 523 and 553 K
// Ea and A are Arrhenius fits to the Kd values, not printed by Fichtl

// Fitted range is about 1600 h
// That is the range of the DECAY LAW and is not a catalyst replacement interval; see economics/catalyst_replacement.hpp

// Source-side inconsistency: Fichtl's text says 4th order at 483 K while
// Table 4's column header reads 493 K
// Table 4 is followed. See docs/24-validation.md

// See docs/19-catalyst-deactivation.md

#include <functional>
#include <string>
#include <vector>

namespace activity_decay {

struct ActivityDecayConfig {
  double m = 3.0; // reaction order
  double Ea_J_per_mol = 0.0;
  double A_per_h = 0.0; // [1/h]

  bool sourced = true;
  const char* source =
      "Derived: Fichtl et al. (2015) Applied Catalysis A 502, 262-270, "
      "Table 4, 2-point Arrhenius fit through the m = 3 points (523, 553 K).";

  // Opt-in water acceleration. 1.0 means dry gas, which is what Table 4 was measured under
  double water_multiplier = 1.0;
  bool water_multiplier_sourced = false;
};

// [1/h]. Throws if T_K <= 0
double Kd_per_h(double T_K, const ActivityDecayConfig& cfg);

// a(t) = [a0^(1-m) + (m-1) Kd t]^(1/(1-m)), constant Kd
// The analytical target the integrator is validated against
// Throws on a0 outside (0,1]
// t_h < 0 or Kd < 0
double activity_closed_form(double a0, double t_h, double Kd_per_h_value, double m);

struct ActivityTrajectoryPoint {
  double t_h = 0.0;
  double a = 1.0; // relative to fresh
  double T_K = 0.0;
};

struct ActivityIntegrationResult {
  std::vector<ActivityTrajectoryPoint> profile;
  double a_final = 1.0;
  bool ok = false;
  std::string message;
};

// Fixed-step RK4 integration of da/dt = -Kd(T(t)) * a^m over [0, t_end_h], following established fixed-step-RK4 convention
// (reactor::integrate_reactor, front_end::integrate_trm_reactor)
// T_of_t is a caller-supplied temperature history in K as a function of time in hours
// T_of_t is a std::function: this is a once-per-campaign call
ActivityIntegrationResult integrate_activity(
    double a0, double t_end_h, int n_steps,
    const std::function<double(double t_h)>& T_of_t,
    const ActivityDecayConfig& cfg);

ActivityIntegrationResult integrate_activity_isothermal(
    double a0, double t_end_h, int n_steps, double T_K,
    const ActivityDecayConfig& cfg);

// Named so every coupling point to lhhw.hpp is greppable
inline double scale_rate(double raw_rate_mol_per_kgcat_s, double activity) {
  return raw_rate_mol_per_kgcat_s * activity;
}

// Presets
namespace presets {

// Two-point Arrhenius fits over Fichtl Table 4's m = 3 points (523, 553 K)
// Derived; the paper prints rate constants, not (A, Ea) pairs
ActivityDecayConfig fichtl_cza1_523_553K(); // Ea 47.422 kJ/mol, A 78.978 /h
ActivityDecayConfig fichtl_cza2_523_553K(); // Ea 39.009 kJ/mol, A 10.544 /h
ActivityDecayConfig fichtl_cza3_523_553K(); // Ea 50.893 kJ/mol, A 59.902 /h

// Fichtl's 493 K CZA1 point, where the fitted order is 4 rather than 3
// Kept standalone: folding it into the fits above would misrepresent a real order change as continuous
// Valid only at 493 K
inline constexpr double kFichtlCza1Kd493K = 4.29e-3;  // [1/h]
inline constexpr double kFichtlCza1M493K = 4.0;

inline constexpr bool kFichtlWaterMultiplierSourced = false;

// Industrial-timescale preset
// FORM sourced to Kordabadi & Jahanmiri (2007) Eq. (4), 5th order, attributed there to Hanken (1995) 
// Kordabadi does not print Kd; it is back-solved from their Fig. 5 anchor below
// Ed is not in Kordabadi either and Fichtl's CZA1 47.422 kJ/mol is used as a stated interim, flagged kIndustrialEdSourced = false
// To close: L. Hanken, MSc thesis, NTNU, 1995 (Kordabadi ref. [19])
ActivityDecayConfig kordabadi_hanken_industrial();

// Calibration anchor, exposed so tests assert against it rather than a copy
inline constexpr double kIndustrialAnchorActivity = 0.40;
inline constexpr double kIndustrialAnchorDays = 1400.0;
inline constexpr double kIndustrialAnchorTempK = 518.15; // 245 C
inline constexpr bool kIndustrialEdSourced = false; // awaiting Hanken

}

} 

