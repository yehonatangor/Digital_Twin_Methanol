// Hydrogen storage: linear density-pressure fit, floor and ceiling
// Mucci et al. (2023) Sec. 3.4

#include "dispatch/storage.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== hydrogen storage ===\n\n");

  auto cfg = dispatch::presets::mucci_h2_storage_defaults();
  cfg.V_m3 = 1000.0;

  std::printf("[1] Sourced defaults\n");
  check("p_min, bar", cfg.p_min_bar, 75.0, 1e-12);
  check("density slope, kg/m3/bar", cfg.rho_slope_kg_per_m3bar, 0.073, 1e-12);
  check("storage temperature, K", cfg.T_K, 298.0, 1e-12);

  std::printf("\n[2] Pressure and mass are exact inverses\n");
  check("empty storage sits at p_min", dispatch::storage_pressure_bar(0.0, cfg), 75.0, 1e-12);
  const double m = dispatch::storage_mass_kg(120.0, cfg);
  check("round trip mass -> pressure", dispatch::storage_pressure_bar(m, cfg), 120.0, 1e-9);
  check("capacity is the mass at p_max",
        dispatch::storage_capacity_kg(cfg), dispatch::storage_mass_kg(160.0, cfg), 1e-12);

  std::printf("\n[3] A normal step just accumulates\n");
  dispatch::StorageState s0; s0.M_H2_kg = 1000.0;
  const auto r1 = dispatch::step_storage(s0, 1.0, 0.5, 3600.0, cfg);
  checkTrue("ok", r1.ok);
  check("net gain over an hour", r1.state.M_H2_kg, 1000.0 + 0.5 * 3600.0, 1e-9);
  checkTrue("no floor violation", !r1.floor_violation);
  checkTrue("no ceiling violation", !r1.ceiling_violation);

  std::printf("\n[4] The floor cannot be breached\n");
  dispatch::StorageState low; low.M_H2_kg = 10.0;
  const auto r2 = dispatch::step_storage(low, 0.0, 5.0, 3600.0, cfg);
  checkTrue("floor violation is reported", r2.floor_violation);
  check("withdrawal is truncated to what was there",
        r2.m_dot_consumed_actual_kg_s, 10.0 / 3600.0, 1e-12);
  check("storage lands exactly empty", r2.state.M_H2_kg, 0.0, 1e-12);
  checkTrue("pressure never falls below p_min", r2.p_bar >= 75.0 - 1e-9);

  std::printf("\n[5] The ceiling vents rather than over-pressuring\n");
  dispatch::StorageState full; full.M_H2_kg = dispatch::storage_capacity_kg(cfg);
  const auto r3 = dispatch::step_storage(full, 10.0, 0.0, 3600.0, cfg);
  checkTrue("ceiling violation is reported", r3.ceiling_violation);
  checkTrue("hydrogen was vented", r3.m_dot_vented_kg_s > 0.0);
  check("storage is pinned at capacity",
        r3.state.M_H2_kg, dispatch::storage_capacity_kg(cfg), 1e-9);
  check("pressure is pinned at p_max", r3.p_bar, 160.0, 1e-9);

  std::printf("\n[6] Guards\n");
  checkTrue("non-positive dt is rejected",
            !dispatch::step_storage(s0, 1.0, 0.0, 0.0, cfg).ok);

  return report("storage");
}
