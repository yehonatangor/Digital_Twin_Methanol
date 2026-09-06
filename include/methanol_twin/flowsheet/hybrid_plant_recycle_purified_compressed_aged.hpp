#pragma once
// The compressed purified plant, plus two things aging adds: an activity
// trajectory and a catalyst-replacement OPEX line
//
// Composition, not an edit. The base result is Chapter 8.7's own, verbatim
//
// The trajectory is informational. The base run is a steady-state design
// point at fresh activity, so the trajectory shows what the catalyst would do
// over time rather than feeding back into that run. Use the aged recycle loop
// when activity has to change the conversion itself
//
// Fichtl's decay law was fitted to roughly 1600 h of laboratory data, so a
// longer horizon is extrapolation and is flagged rather than silently run

#include <string>
#include <vector>

#include "degradation/activity_decay.hpp"
#include "economics/catalyst_replacement.hpp"
#include "flowsheet/hybrid_plant_recycle_purified_compressed.hpp"

namespace flowsheet {

// Fichtl's own fitted window, past which the trajectory is extrapolated
inline constexpr double kFichtlFittedRangeHours = 1600.0;

struct AgingDisplayConfig {
  activity_decay::ActivityDecayConfig decay_cfg =
      activity_decay::presets::fichtl_cza1_523_553K();
  double t_end_h = 1600.0;      // default stays inside the fitted range
  int    n_steps = 200;
  double aging_T_K = 0.0;       // 0 means "use the reactor's own outlet temperature"
};

struct CatalystReplacementConfig {
  double price_USD_per_kg = economics::kCatalystPricePlaceholder_USD_per_kg;
  double lifetime_years   = economics::kCatalystReplacementIntervalPlaceholder_years;
};

struct HybridPlantRecyclePurifiedCompressedAgedConfig {
  HybridPlantRecyclePurifiedCompressedConfig base_cfg;
  AgingDisplayConfig aging_display_cfg;
  CatalystReplacementConfig catalyst_replacement_cfg;
};

struct HybridPlantRecyclePurifiedCompressedAgedResult {
  HybridPlantRecyclePurifiedCompressedResult base;   // Chapter 8.7's own result, unchanged

  // Activity-decay trajectory, informational only
  activity_decay::ActivityIntegrationResult activity_trajectory;
  double aging_temperature_K = 0.0;
  bool   trajectory_within_fichtl_fitted_range = true;

  // Catalyst replacement as an OPEX line
  double catalyst_mass_total_kg = 0.0;
  economics::CatalystReplacementResult catalyst_replacement;
  economics::PlantEconomicsResult plant_economics_with_catalyst;

  bool ok = false;
  std::string message;
};

HybridPlantRecyclePurifiedCompressedAgedResult run_hybrid_plant_recycle_purified_compressed_aged(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantRecyclePurifiedCompressedAgedConfig& cfg =
        HybridPlantRecyclePurifiedCompressedAgedConfig{});

}  // namespace flowsheet
