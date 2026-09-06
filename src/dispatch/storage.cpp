#include "storage.hpp"

#include <algorithm>

namespace dispatch {

// Mucci Sec. 3.4: linear density-pressure fit at 298 K, referenced to p_min
double storage_pressure_bar(double M_H2_kg, const StorageConfig& cfg) {
  if (!(cfg.V_m3 > 0.0) || !(cfg.rho_slope_kg_per_m3bar > 0.0)) return cfg.p_min_bar;
  return cfg.p_min_bar + M_H2_kg / (cfg.V_m3 * cfg.rho_slope_kg_per_m3bar);
}

double storage_mass_kg(double p_bar, const StorageConfig& cfg) {
  if (!(cfg.V_m3 > 0.0)) return 0.0;
  return (p_bar - cfg.p_min_bar) * cfg.V_m3 * cfg.rho_slope_kg_per_m3bar;
}

double storage_capacity_kg(const StorageConfig& cfg) {
  return storage_mass_kg(cfg.p_max_bar, cfg);
}

StorageStepResult step_storage(const StorageState& prev, double m_dot_prod_kg_s,
                                double m_dot_consumed_requested_kg_s, double dt_s,
                                const StorageConfig& cfg) {
  StorageStepResult r;
  r.state = prev;

  if (!(dt_s > 0.0)) {
    r.ok = false;
    r.message = "dt_s must be positive";
    return r;
  }

  const double produced = std::max(0.0, m_dot_prod_kg_s) * dt_s;
  const double requested = std::max(0.0, m_dot_consumed_requested_kg_s) * dt_s;

  const double available = prev.M_H2_kg + produced;
  double consumed = requested;
  if (consumed > available) {
    // Floor cannot withdraw below the p_min reference
    consumed = available;
    r.floor_violation = true;
    r.message = "requested consumption exceeded stored plus produced hydrogen";
  }
  r.m_dot_consumed_actual_kg_s = consumed / dt_s;

  double M = available - consumed;

  // Ceiling vent whatever will not fit below p_max
  const double cap = storage_capacity_kg(cfg);
  if (cap > 0.0 && M > cap) {
    const double vented = M - cap;
    M = cap;
    r.ceiling_violation = true;
    r.m_dot_vented_kg_s = vented / dt_s;
    if (r.message[0] == '\0') r.message = "storage ceiling reached; excess hydrogen vented";
  }

  r.state.M_H2_kg = M;
  r.p_bar = storage_pressure_bar(M, cfg);
  return r;
}

double storage_capex_USD_per_m3(double cost_per_kg_USD, double p_ref_bar, double p_base_bar,
                                 double update_factor, double rho_slope_kg_per_m3bar) {
  // Cost per m^3 = cost per kg times the kg a m^3 holds between the two pressures, escalated by the stated update factor
  const double kg_per_m3 = rho_slope_kg_per_m3bar * (p_ref_bar - p_base_bar);
  return cost_per_kg_USD * kg_per_m3 * update_factor;
}

namespace presets {

StorageConfig mucci_h2_storage_defaults() {
  return StorageConfig{}; // defaults already are Mucci Sec. 3.4
}

}

}
