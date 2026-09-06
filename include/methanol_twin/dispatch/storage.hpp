#pragma once
// Hydrogen storage vessel: mass balance, pressure relation, vessel CAPEX

// Mucci et al., J. Energy Storage 72 (2023) 108614, Sec. 3.4 and App. A.2/A.3
//   M(t) = M(t-dt) + m_prod*dt - m_consumed*dt
//   M(t) = V * 0.073 kg/(m3.bar) * (p(t) - 75 bar)        [linear, 298 K fit]
//   p(t) = p_min + M(t) / (V * 0.073)
//   Cost = 500 $/kg * 0.073 * (160 - 1) bar * 1.35        [$/m3, App. A.3]
// 75 bar is the synthesis pressure, so it is the delivery floor. Vessel cost
// gives 7834.7 $/m3 against Mucci's stated "around 7800"

// V has no default; Mucci treats it as an optimisation variable
// See docs/22-storage-and-dispatch.md

namespace dispatch {

struct StorageConfig {
  double p_min_bar = 75.0; // SOURCED, Sec. 3.4: methanol synthesis plant pressure
  double rho_slope_kg_per_m3bar = 0.073; // SOURCED, Sec. 3.4/Fig. A.2, linear H2 density-pressure fit (298 K)
  double T_K = 298.0; // SOURCED, Sec. 3.4: storage assumed isothermal at 298 K
  double V_m3 = 0.0; // vessel volume -- caller-supplied design variable, no default

  // Ceiling. INFERRED, not stated as an operating limit: 160 bar is Mucci's vessel COST basis (App. A.3),
  // used so the mass balance is consistent
  // with the CAPEX. Without it the demo charged to 621 bar and reported no violation
  // Set p_max <= p_min to disable
  double p_max_bar = 160.0;
  static constexpr bool p_max_from_cost_basis = true; // inferred
};

struct StorageState {
  double M_H2_kg = 0.0; // hydrogen mass currently stored, referenced to the p_min floor (M_H2=0 <=> p=p_min)
};

struct StorageStepResult {
  StorageState state; // updated state after this step
  double m_dot_consumed_actual_kg_s = 0.0; // actual withdrawal used (may be less than requested, see floor_violation)
  double p_bar = 0.0; // resulting pressure, storage_pressure_bar(state.M_H2_kg, cfg)
  bool   floor_violation = false; // true: requested consumption exceeded available stored + produced mass this step

  // Vessel hit p_max and could not absorb all production this step. Surplus is
  // reported rather than silently accumulated; the schedule is infeasible
  bool ceiling_violation = false;
  double m_dot_vented_kg_s = 0.0;

  bool ok = true;
  const char* message = "";
};

// p(t) = p_min + M_H2(t) / (V * rho_slope), clamped at p_min (Sec. 3.4)
// Returns p_min directly if cfg.V_m3 <= 0 (no vessel configured)
double storage_pressure_bar(double M_H2_kg, const StorageConfig& cfg);

// Inverse of storage_pressure_bar(): M_H2 = V * rho_slope * (p - p_min)
double storage_mass_kg(double p_bar, const StorageConfig& cfg);

// Working capacity, kg: the mass held between p_min_bar and p_max_bar
// Returns a negative value if no ceiling is configured (p_max <= p_min),
// meaning "unbounded", callers should test for that rather than assume
double storage_capacity_kg(const StorageConfig& cfg);

// Sec. 3.4 mass balance, one timestep. A withdrawal below the floor is
// truncated to what was available and flagged floor_violation
StorageStepResult step_storage(const StorageState& prev, double m_dot_prod_kg_s,
                                double m_dot_consumed_requested_kg_s, double dt_s,
                                const StorageConfig& cfg);

// App. A.3. Defaults reproduce Mucci's worked case; override for another
// reference design point
double storage_capex_USD_per_m3(double cost_per_kg_USD = 500.0, double p_ref_bar = 160.0,
                                 double p_base_bar = 1.0, double update_factor = 1.35,
                                 double rho_slope_kg_per_m3bar = 0.073);

inline double storage_capex_USD(double V_m3, double capex_per_m3_USD) {
  return V_m3 * capex_per_m3_USD;
}

// Presets
namespace presets {

// Sec. 3.4 defaults. V_m3 left at 0: the caller sizes the vessel
StorageConfig mucci_h2_storage_defaults();

}

}
